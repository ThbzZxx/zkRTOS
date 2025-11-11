/**
 * @file    zk_hook.h
 * @brief   ZK-RTOS hook function interface
 * @note    Hook functions allow users to insert custom code when critical events occur
 */

#ifndef ZK_HOOK_H
#define ZK_HOOK_H

#include "zk_def.h"

#ifdef ZK_USING_HOOK

/* ==================== Hook function type definitions ==================== */

/**
 * @brief Idle task hook function type
 * @note  Called periodically in idle task, can be used for low power mode or background tasks
 */
typedef void (*idle_hook_t)(void);

/**
 * @brief Task switch hook function type
 * @param from_tcb TCB of the task being switched out
 * @param to_tcb TCB of the task being switched in
 * @note  Called during task switch (in PendSV interrupt), keep it short
 */
typedef void (*task_switch_hook_t)(task_control_block_t *from_tcb, task_control_block_t *to_tcb);

/**
 * @brief Tick hook function type
 * @note  Called at each system tick (in SysTick interrupt), keep it short
 */
typedef void (*tick_hook_t)(void);

/**
 * @brief Stack overflow hook function type
 * @param tcb TCB of the task with stack overflow
 * @note  Called when stack overflow is detected, can be used for error logging or entering safe mode
 */
typedef void (*stack_overflow_hook_t)(task_control_block_t *tcb);

/**
 * @brief Memory allocation failure hook function type
 * @param size Size of memory requested to be allocated
 * @note  Called when memory allocation fails, can be used for error handling or logging
 */
typedef void (*malloc_failed_hook_t)(zk_uint32 size);


/* ==================== Hook function registration interface ==================== */

/**
 * @brief Register idle task hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_idle(idle_hook_t hook);

/**
 * @brief Register task switch hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_task_switch(task_switch_hook_t hook);

/**
 * @brief Register tick hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_tick(tick_hook_t hook);

/**
 * @brief Register stack overflow hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_stack_overflow(stack_overflow_hook_t hook);

/**
 * @brief Register memory allocation failure hook
 * @param hook Hook function pointer, pass NULL to unregister
 */
void zk_hook_register_malloc_failed(malloc_failed_hook_t hook);


/* ==================== Internal call interface (users should not call directly) ==================== */

/**
 * @brief Call idle task hook
 */
void zk_hook_call_idle(void);

/**
 * @brief Call task switch hook
 */
void zk_hook_call_task_switch(task_control_block_t *from_tcb, task_control_block_t *to_tcb);

/**
 * @brief Call tick hook
 */
void zk_hook_call_tick(void);

/**
 * @brief Call stack overflow hook
 */
void zk_hook_call_stack_overflow(task_control_block_t *tcb);

/**
 * @brief Call memory allocation failure hook
 */
void zk_hook_call_malloc_failed(zk_uint32 size);

#endif /* ZK_USING_HOOK */

#endif /* ZK_HOOK_H */
