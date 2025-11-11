/**
 * @file    zk_internal.h
 * @brief   ZK-RTOS kernel internal function declarations
 * @note    For kernel source files only, not visible to users
 */

#ifndef ZK_INTERNAL_H
#define ZK_INTERNAL_H

#include "zk_def.h"
#include "zk_cpu_cm3.h" /* Include inline critical section functions */
#include "zk_cpu.h"     /* Include CPU abstraction layer macro definitions */


/* ==================== Time management internal functions ==================== */
zk_uint32 get_current_time(void);
void increment_time(void);
zk_uint32 get_total_run_time(void); /* P1: Get system total runtime */

/* ==================== Scheduler internal functions ==================== */
void schedule(void);
zk_bool is_scheduler_suspending(void);
task_control_block_t *get_highest_priority_task(void);
zk_uint32 scheduler_increment_tick(void);
void zk_start_scheduler(void); /* System startup function (implemented in zk_board.c) */

/* ==================== Task management internal functions ==================== */
void idle_task_create(void);
zk_error_code_t task_create(task_init_parameter_t *parameter, zk_uint32 *task_handle);
void add_task_to_ready_list(task_control_block_t *tcb);
void remove_task_from_ready_list(task_control_block_t *tcb);

/* Task state transition functions */
void task_ready_to_delay(task_control_block_t *tcb);
void task_ready_to_suspend(task_control_block_t *tcb);
void task_suspend_to_ready(task_control_block_t *tcb);
void task_ready_to_block(task_control_block_t *tcb, zk_list_node_t *sleep_list_head,
						 block_type_t block_type, block_sort_type_t sort_type);
void task_block_to_ready(task_control_block_t *tcb);

/* Temporary task priority modification (for priority inheritance) */
void task_change_priority_temp(task_control_block_t *tcb, zk_uint8 new_priority);
void task_resume_priority(task_control_block_t *tcb);

/* ==================== Memory management internal functions ==================== */
void *mem_alloc(zk_uint32 size);
void mem_free(void *addr);

/* ==================== Timer internal functions ==================== */
#ifdef ZK_USING_TIMER
void timer_check(zk_uint32 current_time);
#endif

/* ==================== Architecture-related functions ==================== */
void *zk_arch_prepare_stack(void *stack_start, void *param);

/* ==================== Utility macros ==================== */
#define TASK_HANDLE_TO_TCB(handle) ((task_control_block_t *) handle)

/* ==================== Parameter validation macros (P0 enhancement) ==================== */
/**
 * @brief Check if pointer is NULL
 * @note  Used to replace ZK_ASSERT_NULL_POINTER, also effective in Release mode
 */
#define ZK_CHECK_PARAM_NOT_NULL(ptr)                                                               \
	do                                                                                             \
	{                                                                                              \
		if ((ptr) == ZK_NULL)                                                                         \
		{                                                                                          \
			return ZK_ERR_INVALID_PARAM;                                                           \
		}                                                                                          \
	} while (0)

/**
 * @brief Check if handle is valid
 * @param handle Handle value
 * @param max Maximum valid value
 */
#define ZK_CHECK_HANDLE_VALID(handle, max)                                                         \
	do                                                                                             \
	{                                                                                              \
		if ((handle) >= (max))                                                                     \
		{                                                                                          \
			return ZK_ERR_INVALID_HANDLE;                                                          \
		}                                                                                          \
	} while (0)

/**
 * @brief Check parameter range
 * @param value Parameter value
 * @param min Minimum value
 * @param max Maximum value
 */
#define ZK_CHECK_RANGE(value, min, max)                                                            \
	do                                                                                             \
	{                                                                                              \
		if ((value) < (min) || (value) > (max))                                                    \
		{                                                                                          \
			return ZK_ERR_OUT_OF_RANGE;                                                            \
		}                                                                                          \
	} while (0)

#endif /* ZK_INTERNAL_H */
