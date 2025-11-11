/**
 * @file    zk_hook.c
 * @brief   ZK-RTOS hook function implementation
 */

#include "zk_internal.h"
#include "zk_hook.h"

#ifdef ZK_USING_HOOK

/* ==================== 全局钩子函数指针 ==================== */

static idle_hook_t g_idle_hook = ZK_NULL;
static task_switch_hook_t g_task_switch_hook = ZK_NULL;
static tick_hook_t g_tick_hook = ZK_NULL;
static stack_overflow_hook_t g_stack_overflow_hook = ZK_NULL;
static malloc_failed_hook_t g_malloc_failed_hook = ZK_NULL;


/* ==================== 钩子函数注册接口 ==================== */

/**
 * @brief Register idle task hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_idle(idle_hook_t hook)
{
	ZK_ENTER_CRITICAL();
	g_idle_hook = hook;
	ZK_EXIT_CRITICAL();
}

/**
 * @brief Register task switch hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_task_switch(task_switch_hook_t hook)
{
	ZK_ENTER_CRITICAL();
	g_task_switch_hook = hook;
	ZK_EXIT_CRITICAL();
}

/**
 * @brief Register tick hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_tick(tick_hook_t hook)
{
	ZK_ENTER_CRITICAL();
	g_tick_hook = hook;
	ZK_EXIT_CRITICAL();
}

/**
 * @brief Register stack overflow hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_stack_overflow(stack_overflow_hook_t hook)
{
	ZK_ENTER_CRITICAL();
	g_stack_overflow_hook = hook;
	ZK_EXIT_CRITICAL();
}

/**
 * @brief Register memory allocation failure hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_malloc_failed(malloc_failed_hook_t hook)
{
	ZK_ENTER_CRITICAL();
	g_malloc_failed_hook = hook;
	ZK_EXIT_CRITICAL();
}


/* ==================== 内部调用接口 ==================== */

/**
 * @brief Call idle task hook
 * @note  Called in idle task
 */
void zk_hook_call_idle(void)
{
	if (g_idle_hook != ZK_NULL)
	{
		g_idle_hook();
	}
}

/**
 * @brief Call task switch hook
 * @param from_tcb TCB of the task being switched out
 * @param to_tcb TCB of the task being switched in
 * @note  Called in PendSV interrupt
 */
void zk_hook_call_task_switch(task_control_block_t *from_tcb, task_control_block_t *to_tcb)
{
	if (g_task_switch_hook != ZK_NULL)
	{
		g_task_switch_hook(from_tcb, to_tcb);
	}
}

/**
 * @brief Call tick hook
 * @note  Called in SysTick interrupt
 */
void zk_hook_call_tick(void)
{
	if (g_tick_hook != ZK_NULL)
	{
		g_tick_hook();
	}
}

/**
 * @brief Call stack overflow hook
 * @param tcb TCB of the task with stack overflow
 * @note  Called when stack overflow is detected
 */
void zk_hook_call_stack_overflow(task_control_block_t *tcb)
{
	if (g_stack_overflow_hook != ZK_NULL)
	{
		g_stack_overflow_hook(tcb);
	}
}

/**
 * @brief Call memory allocation failure hook
 * @param size Requested memory allocation size
 * @note  Called when memory allocation fails
 */
void zk_hook_call_malloc_failed(zk_uint32 size)
{
	if (g_malloc_failed_hook != ZK_NULL)
	{
		g_malloc_failed_hook(size);
	}
}

#endif /* ZK_USING_HOOK */
