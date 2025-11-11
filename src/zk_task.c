/**
 * @file    zk_task.c
 * @brief   ZK-RTOS task management module
 */

#include "zk_internal.h"
#ifdef ZK_USING_HOOK
#include "zk_hook.h"
#endif

extern task_scheduler_t g_scheduler;

task_control_block_t *volatile g_current_tcb = ZK_NULL;
task_control_block_t *volatile g_switch_next_tcb = ZK_NULL;
static zk_uint32 g_idle_task_handle = 0;

zk_error_code_t task_create(task_init_parameter_t *parameter, zk_uint32 *task_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	task_control_block_t *tcb = ZK_NULL;
	void *stack_mem = ZK_NULL;

	ZK_ASSERT_NULL_POINTER(parameter);
	ZK_ASSERT_NULL_POINTER(task_handle);

	ZK_ASSERT_PARAM(parameter->priority <= MIN_TASK_PRIORITY);

	tcb = (task_control_block_t *) mem_alloc(sizeof(task_control_block_t));
	if (tcb == ZK_NULL)
	{
		return ZK_ERR_NOT_ENOUGH_MEMORY;
	}

	stack_mem = mem_alloc(parameter->stack_size);
	if (stack_mem == ZK_NULL)
	{
		mem_free(tcb);
		return ZK_ERR_NOT_ENOUGH_MEMORY;
	}

	zk_memset(stack_mem, TASK_MAGIC_NUMBER, parameter->stack_size);

	tcb->base_priority = parameter->priority;
	tcb->priority = parameter->priority;
	zk_memcpy(tcb->task_name, parameter->name, CONFIG_TASK_NAME_LEN);
	tcb->task_name[CONFIG_TASK_NAME_LEN - 1] = ZK_STRING_TERMINATOR;

	tcb->stack_base = stack_mem;
	tcb->stack_size = parameter->stack_size;

	tcb->run_time_ticks = 0;
	tcb->last_switch_in_time = 0;

#ifdef ZK_USING_MUTEX
	tcb->holding_mutex = ZK_NULL;
#endif

	tcb->stack = zk_arch_prepare_stack(stack_mem, parameter);
	tcb->state = TASK_UNKNOWN;
	tcb->wake_up_time = ZK_TIME_MAX;

	ZK_ENTER_CRITICAL();

	add_task_to_ready_list(tcb);

	*task_handle = (zk_uint32) tcb;

	ZK_EXIT_CRITICAL();

	return ZK_SUCCESS;
}


/**
 * @brief Get the highest priority ready task
 * @return TCB pointer of the highest priority task
 * @note Optimization: uses CLZ instruction for O(1) time complexity (optimized from O(n))
 */
task_control_block_t *get_highest_priority_task(void)
{
	task_control_block_t *highest_ready_tcb = ZK_NULL;

	/* 使用CLZ硬件指令快速查找最高优先级 */
	zk_uint8 leading_zeros = zk_cpu_clz(g_scheduler.priority_active);
	zk_uint8 highest_priority = leading_zeros;

	highest_ready_tcb = ZK_LIST_GET_FIRST_ENTRY(&g_scheduler.ready_list[highest_priority],
												task_control_block_t, state_node);

	return highest_ready_tcb;
}


void idle_task(void *parameter)
{
	// Idle task runs when no other tasks are ready
	// Parameter is unused
	while (1)
	{
#ifdef ZK_USING_HOOK
		/* 调用空闲任务钩子 */
		zk_hook_call_idle();
#endif
		// In actual implementation, this would typically include
		// low power mode handling or background resource reclamation
	}
}
void idle_task_create(void)
{
	task_init_parameter_t *parameter =
		(task_init_parameter_t *) mem_alloc(sizeof(task_init_parameter_t));
	ZK_ASSERT_NULL_POINTER(parameter);

	parameter->name[0] = 'I';
	parameter->name[1] = 'D';
	parameter->name[2] = 'L';
	parameter->name[3] = 'E';
	parameter->name[4] = ZK_STRING_TERMINATOR;
	// Idle task has lowest priority
	parameter->priority = IDLE_TASK_PRIO;
	parameter->private_data = ZK_NULL;
	parameter->stack_size = IDLE_TASK_STACK_SIZE;
	parameter->task_entry = idle_task;
	task_create(parameter, &g_idle_task_handle);
	mem_free(parameter);
}

zk_error_code_t task_delay(zk_uint32 delay_time)
{
	zk_error_code_t ret = ZK_SUCCESS;
	ZK_ENTER_CRITICAL();
	if (is_scheduler_suspending())
	{
		ret = ZK_ERR_STATE;
		goto task_delay_exit;
	}

	ZK_ASSERT_PARAM((delay_time > 0) && (delay_time < ZK_TSK_DLY_MAX));
	g_current_tcb->wake_up_time = get_current_time() + delay_time;
	task_ready_to_delay(g_current_tcb);
	schedule();

task_delay_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

void task_change_priority_temp(task_control_block_t *tcb, zk_uint8 new_priority)
{
	tcb->priority = new_priority;
	if (tcb->state == TASK_READY)
	{
		remove_task_from_ready_list(tcb);
		add_task_to_ready_list(tcb);
	}
}

void task_resume_priority(task_control_block_t *tcb)
{
	tcb->priority = tcb->base_priority;
	if (tcb->state == TASK_READY)
	{
		remove_task_from_ready_list(tcb);
		add_task_to_ready_list(tcb);
	}
}

/**
 * @brief   Check if task stack has overflowed
 * @param   tcb Task control block pointer
 * @return  ZK_TRUE: stack overflow, ZK_FALSE: normal
 */
zk_bool task_check_stack_overflow(task_control_block_t *tcb)
{
	zk_uint8 *stack_bottom = (zk_uint8 *) tcb->stack_base;
	zk_uint32 check_size = 16;

	if (check_size > tcb->stack_size)
	{
		check_size = tcb->stack_size;
	}

	for (zk_uint32 i = 0; i < check_size; i++)
	{
		if (stack_bottom[i] != TASK_MAGIC_NUMBER)
		{
#ifdef ZK_USING_HOOK
			zk_hook_call_stack_overflow(tcb);
#endif
			return ZK_TRUE;
		}
	}

	return ZK_FALSE;
}

/**
 * @brief   Calculate task stack usage
 * @param   tcb Task control block pointer
 * @return  Stack usage in bytes
 */
zk_uint32 task_get_stack_usage(task_control_block_t *tcb)
{
	zk_uint8 *stack_ptr = (zk_uint8 *) tcb->stack_base;
	zk_uint32 unused = 0;

	for (zk_uint32 i = 0; i < tcb->stack_size; i++)
	{
		if (stack_ptr[i] == TASK_MAGIC_NUMBER)
		{
			unused++;
		}
		else
		{
			break;
		}
	}

	return tcb->stack_size - unused;
}

/**
 * @brief   Update task runtime statistics (called during context switch)
 * @param   old_tcb TCB of the task being switched out
 * @param   new_tcb TCB of the task being switched in
 */
void task_update_runtime_stats(task_control_block_t *old_tcb, task_control_block_t *new_tcb)
{
	zk_uint32 current_time = get_total_run_time();

	if (old_tcb != ZK_NULL && old_tcb->last_switch_in_time > 0)
	{
		zk_uint32 delta = current_time - old_tcb->last_switch_in_time;
		old_tcb->run_time_ticks += delta;
	}

	if (new_tcb != ZK_NULL)
	{
		new_tcb->last_switch_in_time = current_time;
	}

#ifdef ZK_USING_HOOK
	zk_hook_call_task_switch(old_tcb, new_tcb);
#endif
}

/**
 * @brief   Get task runtime
 * @param   tcb Task control block pointer
 * @return  Task accumulated runtime in ticks
 */
zk_uint32 task_get_runtime(task_control_block_t *tcb)
{
	zk_uint32 runtime = 0;

	ZK_ENTER_CRITICAL();
	runtime = tcb->run_time_ticks;
	ZK_EXIT_CRITICAL();

	return runtime;
}

/**
 * @brief   Get task CPU usage
 * @param   tcb Task control block pointer
 * @return  CPU usage (percentage * 100)
 */
zk_uint32 task_get_cpu_usage(task_control_block_t *tcb)
{
	zk_uint32 usage = 0;
	zk_uint32 total_time;
	zk_uint32 task_time;

	ZK_ENTER_CRITICAL();

	total_time = get_total_run_time();
	task_time = tcb->run_time_ticks;

	if (total_time == 0)
	{
		ZK_EXIT_CRITICAL();
		return 0;
	}

	usage = (task_time * 10000) / total_time;

	ZK_EXIT_CRITICAL();

	return usage;
}
