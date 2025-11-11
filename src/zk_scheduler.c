/**
 * @file    zk_scheduler.c
 * @brief   scheduler management module
 * @note    implement priority preemptive scheduling algorithm
 */

#include "zk_internal.h"
#include "zk_cpu_cm3.h"
#ifdef ZK_USING_HOOK
#include "zk_hook.h"
#endif

extern task_control_block_t *volatile g_current_tcb;
extern task_control_block_t *volatile g_switch_next_tcb;

/* global scheduler */
task_scheduler_t g_scheduler;
/* schedule time slice */
static zk_uint32 g_schedule_time_slice = CONFIG_TICK_COUNT_INIT_VALUE;

/**
 * @brief Initialize scheduler
 */
void scheduler_init(void)
{
	for (int i = 0; i < ZK_PRIORITY_NUM; i++)
	{
		zk_list_init(&g_scheduler.ready_list[i]);
	}
	zk_list_init(&g_scheduler.delay_list);
	zk_list_init(&g_scheduler.suspend_list);
	zk_list_init(&g_scheduler.block_timeout_list);

	g_scheduler.scheduler_suspend_nesting = 0;
	g_scheduler.priority_active = 0;
	g_scheduler.re_schedule_pending = SCHEDULE_PENDING_NONE;
	g_schedule_time_slice = SCHEDULE_TIME_SLICE_INIT_VALUE;
}

/**
 * @brief Check if scheduler is suspending
 * @return zk_uint8 1 if suspending, otherwise 0
 */
zk_uint8 is_scheduler_suspending(void)
{
	zk_uint8 is_suspending = 0;
	ZK_ENTER_CRITICAL();
	is_suspending = g_scheduler.scheduler_suspend_nesting;
	ZK_EXIT_CRITICAL();
	return is_suspending;
}

/**
 * @brief Schedule next task
 * @note This function can be called from multiple contexts:
 *       1. SysTick interrupt (g_switch_next_tcb already set)
 *       2. task_delay/task_suspend (g_switch_next_tcb may not be set)
 *       3. Semaphore/Mutex/Queue operations
 */
void schedule(void)
{
	zk_uint8 need_switch = 0;

	if (is_scheduler_suspending())
	{
		g_scheduler.re_schedule_pending = 1;
		return;
	}

	g_switch_next_tcb = get_highest_priority_task();

	if (g_switch_next_tcb->priority != g_current_tcb->priority)
	{
		need_switch = 1;
		goto schedule_now;
	}
	else
	{
		zk_list_node_t *ready_list_head = &g_scheduler.ready_list[g_current_tcb->priority];
		if (g_current_tcb->state_node.next == ready_list_head &&
			g_current_tcb->state_node.pre == ready_list_head)
		{
			need_switch = 0;
		}
		else
		{
			need_switch = 1;
			zk_list_move_to_tail(&g_current_tcb->state_node, ready_list_head);
			g_switch_next_tcb =
				ZK_LIST_GET_OWNER(ready_list_head->next, task_control_block_t, state_node);
		}
	}

schedule_now:
	if (need_switch == 1)
	{
		zk_cpu_trigger_pendsv();
	}
}
/**
 * @brief Start scheduler
 */
void start_scheduler(void)
{
	g_current_tcb = get_highest_priority_task();
	g_current_tcb->last_switch_in_time = get_total_run_time();

	zk_cpu_start_scheduler();
}
/**
 * @brief Clear priority active bit
 * @param priority Priority level
 */
void clear_priority_active(zk_uint8 priority)
{
	g_scheduler.priority_active &= (~(ZK_BIT_MASK_0 << priority));
}
/**
 * @brief Set priority active bit
 * @param priority Priority level
 */
void set_priority_active(zk_uint8 priority)
{
	g_scheduler.priority_active |= (ZK_BIT_MASK_0 << priority);
}


/**
 * @brief Add task to endless block list
 * @param tcb Task control block
 * @param sleep_head Sleep list head
 * @param sort_type Block sort type
 */
void add_task_to_endless_block_list(task_control_block_t *tcb, zk_list_node_t *sleep_head,
									block_sort_type_t sort_type)
{
	zk_list_node_t *iterator = LIST_NODE_NULL;
	task_control_block_t *tcb_iterator = ZK_NULL;

	switch (sort_type)
	{
	case BLOCK_SORT_FIFO:
		zk_list_add_after(&tcb->event_sleep_list, sleep_head);
		break;

	case BLOCK_SORT_PRIO:
		if (zk_list_is_empty(sleep_head))
		{
			zk_list_add_after(&tcb->event_sleep_list, sleep_head);
		}
		else
		{
			ZK_LIST_FOR_EACH_NODE(iterator, sleep_head)
			{
				tcb_iterator = ZK_LIST_GET_OWNER(iterator, task_control_block_t, event_sleep_list);
				if (tcb_iterator->priority < tcb->priority)
				{
					break;
				}
			}

			if (iterator == sleep_head)
			{
				zk_list_add_after(&tcb->event_sleep_list, &tcb_iterator->event_sleep_list);
			}
			else
			{
				zk_list_add_before(&tcb->event_sleep_list, &tcb_iterator->event_sleep_list);
			}
		}
		break;
	}

	tcb->state = TASK_ENDLESS_BLOCKED;
}

/**
 * @brief Add task to time-sorted list
 * @param tcb Task control block
 * @param target_list Target list type
 */
void add_task_to_time_sort_list(task_control_block_t *tcb, scheduler_state_list_t target_list)
{
	zk_list_node_t *iterator = LIST_NODE_NULL;
	task_control_block_t *tcb_iterator = ZK_NULL;
	zk_list_node_t *target_list_head = ZK_NULL;

	switch (target_list)
	{
	case DELAY_LIST:
		target_list_head = &g_scheduler.delay_list;
		break;

	case BLOCKED_TIMEOUT_LIST:
		target_list_head = &g_scheduler.block_timeout_list;
		break;

	default:
		target_list_head = &g_scheduler.delay_list;
		break;
	}

	if (zk_list_is_empty(target_list_head))
	{
		zk_list_add_after(&tcb->state_node, target_list_head);
	}
	else
	{
		ZK_LIST_FOR_EACH_NODE(iterator, target_list_head)
		{
			tcb_iterator = ZK_LIST_GET_OWNER(iterator, task_control_block_t, state_node);
			/* Use overflow-safe time comparison */
			if (zk_time_is_reached(tcb_iterator->wake_up_time, tcb->wake_up_time))
			{
				break;
			}
		}

		if (iterator == target_list_head)
		{
			zk_list_add_before(&tcb->state_node, target_list_head);
		}
		else
		{
			zk_list_add_before(&tcb->state_node, &tcb_iterator->state_node);
		}
	}
}


/**
 * @brief Add task to ready list
 * @param tcb Task control block
 */
void add_task_to_ready_list(task_control_block_t *tcb)
{
	zk_list_add_after(&tcb->state_node, &g_scheduler.ready_list[tcb->priority]);
	set_priority_active(tcb->priority);
	tcb->state = TASK_READY;
}

/**
 * @brief Add task to timeout blocked list
 * @param tcb Task control block
 */
void add_task_to_timeout_blocked_list(task_control_block_t *tcb)
{
	add_task_to_time_sort_list(tcb, BLOCKED_TIMEOUT_LIST);
	tcb->state = TASK_TIMEOUT_BLOCKED;
}

/**
 * @brief Add task to suspend list
 * @param tcb Task control block
 */
void add_task_to_suspend_list(task_control_block_t *tcb)
{
	zk_list_add_after(&tcb->state_node, &g_scheduler.suspend_list);
	tcb->state = TASK_SUSPEND;
}

/**
 * @brief Add task to delay list
 * @param tcb Task control block
 */
void add_task_to_delay_list(task_control_block_t *tcb)
{
	add_task_to_time_sort_list(tcb, DELAY_LIST);
	tcb->state = TASK_DELAY;
}

/**
 * @brief Remove task from ready list
 * @param tcb Task control block
 */
void remove_task_from_ready_list(task_control_block_t *tcb)
{
	zk_list_delete(&tcb->state_node);
	if (zk_list_is_empty(&(g_scheduler.ready_list[tcb->priority])))
	{
		clear_priority_active(tcb->priority);
	}
	tcb->state = TASK_UNKNOWN;
}

/**
 * @brief Remove task from delay list
 * @param tcb Task control block
 */
void remove_task_from_delay_list(task_control_block_t *tcb)
{
	zk_list_delete(&tcb->state_node);
	tcb->state = TASK_UNKNOWN;
}

/**
 * @brief Remove task from suspend list
 * @param tcb Task control block
 */
void remove_task_from_suspend_list(task_control_block_t *tcb)
{
	zk_list_delete(&tcb->state_node);
	tcb->state = TASK_UNKNOWN;
}

/**
 * @brief Remove task from blocked list
 * @param tcb Task control block
 */
void remove_task_from_blocked_list(task_control_block_t *tcb)
{
	if (tcb->state == TASK_TIMEOUT_BLOCKED)
	{
		zk_list_delete(&tcb->state_node);
	}
	zk_list_delete(&tcb->event_sleep_list);
	tcb->state = TASK_UNKNOWN;
}

/**
 * @brief Remove task from timeout blocked list
 * @param tcb Task control block
 */
void remove_task_from_timeout_blocked_list(task_control_block_t *tcb)
{
	zk_list_delete(&tcb->state_node);
	tcb->state = TASK_UNKNOWN;
}

/**
 * @brief Move task from ready list to delay list
 * @param tcb Task control block
 */
void task_ready_to_delay(task_control_block_t *tcb)
{
	remove_task_from_ready_list(tcb);
	add_task_to_delay_list(tcb);
}

/**
 * @brief Move task from delay list to ready list
 * @param tcb Task control block
 */
void task_delay_to_ready(task_control_block_t *tcb)
{
	remove_task_from_delay_list(tcb);
	add_task_to_ready_list(tcb);
}

/**
 * @brief Move task from ready list to blocked list
 * @param tcb Task control block
 * @param sleep_head Sleep list head
 * @param block_type Block type
 * @param sort_type Sort type
 */
void task_ready_to_block(task_control_block_t *tcb, zk_list_node_t *sleep_head,
						 block_type_t block_type, block_sort_type_t sort_type)
{
	remove_task_from_ready_list(tcb);
	add_task_to_endless_block_list(tcb, sleep_head, sort_type);
	if (block_type == BLOCK_TYPE_TIMEOUT)
	{
		add_task_to_timeout_blocked_list(tcb);
	}
}

/**
 * @brief Move task from blocked list to ready list
 * @param tcb Task control block
 */
void task_block_to_ready(task_control_block_t *tcb)
{
	remove_task_from_blocked_list(tcb);
	add_task_to_ready_list(tcb);
}

/**
 * @brief Move task from ready list to suspend list
 * @param tcb Task control block
 */
void task_ready_to_suspend(task_control_block_t *tcb)
{
	remove_task_from_ready_list(tcb);
	add_task_to_suspend_list(tcb);
}

/**
 * @brief Move task from suspend list to ready list
 * @param tcb Task control block
 */
void task_suspend_to_ready(task_control_block_t *tcb)
{
	remove_task_from_suspend_list(tcb);
	add_task_to_ready_list(tcb);
}

/**
 * @brief Move task from suspend list to blocked list
 * @param tcb Task control block
 * @param sleep_head Sleep list head
 * @param block_type Block type
 * @param sort_type Sort type
 */
void task_suspend_to_block(task_control_block_t *tcb, zk_list_node_t *sleep_head,
						   block_type_t block_type, block_sort_type_t sort_type)
{
	remove_task_from_suspend_list(tcb);
	add_task_to_endless_block_list(tcb, sleep_head, sort_type);
	if (block_type == BLOCK_TYPE_TIMEOUT)
	{
		add_task_to_timeout_blocked_list(tcb);
	}
}

/**
 * @brief Check and wake up delayed tasks
 * @param time Current time
 */
void check_delay_task_wakeup(zk_uint32 time)
{
	if (zk_list_is_empty(&g_scheduler.delay_list))
	{
		return;
	}

	zk_list_node_t *iterator = ZK_NULL;
	zk_list_node_t *iterator_prev = ZK_NULL;
	task_control_block_t *tcb_iterator = ZK_NULL;

	ZK_LIST_FOR_EACH_NODE(iterator, &g_scheduler.delay_list)
	{
		tcb_iterator = ZK_LIST_GET_OWNER(iterator, task_control_block_t, state_node);
		/* Use overflow-safe time comparison */
		if (zk_time_is_reached(time, tcb_iterator->wake_up_time))
		{
			iterator_prev = iterator->pre;
			task_delay_to_ready(tcb_iterator);
			iterator = iterator_prev;
		}
		else
		{
			break;
		}
	}
}
/**
 * @brief Check and wake up timeout blocked tasks
 * @param time Current time
 */
void check_task_block_wakeup(zk_uint32 time)
{
	if (zk_list_is_empty(&g_scheduler.block_timeout_list))
	{
		return;
	}

	zk_list_node_t *iterator = ZK_NULL;
	zk_list_node_t *iterator_prev = ZK_NULL;
	task_control_block_t *tcb_iterator = ZK_NULL;

	ZK_LIST_FOR_EACH_NODE(iterator, &g_scheduler.block_timeout_list)
	{
		tcb_iterator = ZK_LIST_GET_OWNER(iterator, task_control_block_t, state_node);

		/* Use overflow-safe time comparison */
		if (zk_time_is_reached(time, tcb_iterator->wake_up_time))
		{
			iterator_prev = iterator->pre;
			tcb_iterator->event_timeout_wakeup = EVENT_WAIT_TIMEOUT;
			task_block_to_ready(tcb_iterator);
			iterator = iterator_prev;
		}
		else
		{
			break;
		}
	}
}

/**
 * @brief Check all task wakeup conditions
 * @param time Current time
 */
void check_task_wakeup(zk_uint32 time)
{
	check_delay_task_wakeup(time);
	check_task_block_wakeup(time);
}
/**
 * @brief Increment tick
 * @return zk_uint32 ZK_TRUE if reschedule needed, otherwise ZK_FALSE
 * @note Fix: Only perform time-slice rotation when there are multiple tasks at the same priority
 */
zk_uint32 scheduler_increment_tick(void)
{
	zk_uint8 need_schedule = ZK_FALSE;
	zk_uint32 current_time = get_current_time();

	ZK_ENTER_CRITICAL();

	if (is_scheduler_suspending())
	{
		goto scheduler_increment_tick_exit;
	}

	increment_time();
	check_task_wakeup(current_time);

	g_switch_next_tcb = get_highest_priority_task();

	if (g_switch_next_tcb->priority < g_current_tcb->priority)
	{
		schedule();
		need_schedule = ZK_TRUE;
		goto scheduler_increment_tick_exit;
	}

	if (g_switch_next_tcb->priority == g_current_tcb->priority)
	{
		zk_list_node_t *ready_list = &g_scheduler.ready_list[g_current_tcb->priority];

		if (!zk_list_is_empty(ready_list) && ready_list->next != ready_list->pre)
		{
			if (--g_schedule_time_slice <= 0)
			{
				g_schedule_time_slice = SCHEDULE_TIME_SLICE_INIT_VALUE;

				zk_list_move_to_tail(&g_current_tcb->state_node, ready_list);

				g_switch_next_tcb =
					ZK_LIST_GET_FIRST_ENTRY(ready_list, task_control_block_t, state_node);

				schedule();
				need_schedule = ZK_TRUE;
			}
		}
		else
		{
			g_schedule_time_slice = SCHEDULE_TIME_SLICE_INIT_VALUE;
		}
	}

scheduler_increment_tick_exit:
	ZK_EXIT_CRITICAL();

#ifdef ZK_USING_TIMER
	timer_check(current_time);
#endif

#ifdef ZK_USING_HOOK
	zk_hook_call_tick();
#endif

	return need_schedule;
}
