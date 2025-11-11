/**
 * @file    zk_rtos.h
 * @brief   ZK-RTOS public API interface header file
 */

#ifndef ZK_RTOS_H
#define ZK_RTOS_H


#include "zk_def.h"
#include <stddef.h>


/* ==================== Global variable declarations ==================== */
extern task_control_block_t *volatile g_current_tcb;

/* ==================== System initialization and startup ==================== */

/**
 * @brief Initialize ZK-RTOS kernel
 * @note  Must be called after board_init() and before zk_start_scheduler()
 *        Initializes all kernel components (memory, scheduler, IPC)
 */
void zk_kernel_init(void);

/**
 * @brief Start ZK-RTOS scheduler (creates idle task and starts first task)
 * @note  Never returns
 */
void zk_start_scheduler(void);

zk_uint32 get_current_time(void);

/**
 * @brief Simple blocking delay function (busy-wait)
 * @param ms Delay time in milliseconds
 * @note  Does NOT use scheduler, simple busy-wait loop
 *        Use for initialization or when scheduler is not running
 *        For tasks, use task_delay() instead
 */
void zk_delay_ms(zk_uint32 ms);


/* ==================== Task management API ==================== */

zk_error_code_t task_create(task_init_parameter_t *parameter, zk_uint32 *task_handle);
zk_bool task_check_stack_overflow(task_control_block_t *tcb);
zk_uint32 task_get_stack_usage(task_control_block_t *tcb);
zk_uint32 task_get_runtime(task_control_block_t *tcb);
zk_uint32 task_get_cpu_usage(task_control_block_t *tcb);
zk_error_code_t task_delay(zk_uint32 delay_time);

/* ==================== Scheduler API ==================== */
void scheduler_init(void);
void start_scheduler(void);

/* ==================== Timer API ==================== */
#ifdef ZK_USING_TIMER
void timer_init(void);
zk_error_code_t timer_create(zk_uint32 *timer_handle, timer_mode_t mode, zk_uint32 interval,
							 timer_handler_t handler, void *param);
zk_error_code_t timer_start(zk_uint32 timer_handle);
zk_error_code_t timer_stop(zk_uint32 timer_handle);
zk_error_code_t timer_delete(zk_uint32 timer_handle);
zk_error_code_t timer_reset(zk_uint32 timer_handle, zk_uint32 new_interval);
zk_error_code_t timer_get_remaining(zk_uint32 timer_handle, zk_uint32 *remaining);
#endif

/* ==================== Semaphore API ==================== */
#ifdef ZK_USING_SEMAPHORE
void sem_init(void);
zk_error_code_t sem_create(zk_uint32 *sem_handle, zk_uint32 initial_count);
zk_error_code_t sem_get(zk_uint32 sem_handle);
zk_error_code_t sem_try_get(zk_uint32 sem_handle);
zk_error_code_t sem_get_timeout(zk_uint32 sem_handle, zk_uint32 timeout);
zk_error_code_t sem_release(zk_uint32 sem_handle);
zk_error_code_t sem_destroy(zk_uint32 sem_handle);
#endif

/* ==================== Mutex API ==================== */
#ifdef ZK_USING_MUTEX
void mutex_init(void);
zk_error_code_t mutex_create(zk_uint32 *MutexHandle);
zk_error_code_t mutex_lock(zk_uint32 MutexHandle);
zk_error_code_t mutex_lock_timeout(zk_uint32 MutexHandle, zk_uint32 Timeout);
zk_error_code_t mutex_try_lock(zk_uint32 MutexHandle);
zk_error_code_t mutex_unlock(zk_uint32 MutexHandle);
zk_error_code_t mutex_destroy(zk_uint32 MutexHandle);
#endif


/* ==================== Message queue API ==================== */
#ifdef ZK_USING_QUEUE
/* Queue management interface */
void queue_init(void);
zk_error_code_t queue_create(zk_uint32 *queue_handle, zk_uint32 element_single_size,
							 zk_uint32 element_num);
zk_error_code_t queue_destroy(zk_uint32 queue_handle);
/* Queue write interface */
zk_error_code_t queue_write(zk_uint32 queue_handle, const void *buffer, zk_uint32 size);
zk_error_code_t queue_try_write(zk_uint32 queue_handle, const void *buffer, zk_uint32 size);
zk_error_code_t queue_write_timeout(zk_uint32 queue_handle, const void *buffer, zk_uint32 size,
									zk_uint32 timeout);
/* Queue read interface */
zk_error_code_t queue_read(zk_uint32 queue_handle, void *buffer, zk_uint32 size);
zk_error_code_t queue_try_read(zk_uint32 queue_handle, void *buffer, zk_uint32 size);
zk_error_code_t queue_read_timeout(zk_uint32 queue_handle, void *buffer, zk_uint32 size,
								   zk_uint32 timeout);
#endif

/* ==================== Memory management API ==================== */
void mem_init(void);
void *mem_alloc(zk_uint32 size);
void mem_free(void *addr);

/* P1: Memory statistics API */
void mem_get_stats(zk_uint32 *total_size, zk_uint32 *used_size, zk_uint32 *peak_used,
				   zk_uint32 *free_blocks, zk_uint32 *alloc_count, zk_uint32 *alloc_fail_count);
zk_uint32 mem_get_fragmentation(void);


/* ==================== Hook function API ==================== */
#ifdef ZK_USING_HOOK
#include "zk_hook.h"
#endif

/* ==================== Print output API ==================== */

/**
 * @brief Lightweight printf implementation
 * @param fmt Format string
 * @note Supported format specifiers: %d (signed) %u (unsigned) %x (hexadecimal) %s (string) %c (character)
 */
void zk_printf(const char *fmt, ...);

/**
 * @brief Character output hook (user can override)
 * @param c Character to output
 * @note User should override this function in BSP layer to implement UART/SWO output
 */
void zk_putc(char c);


#endif
