/**
 * @file    zk_mutex.c
 * @brief   mutex management module
 * @note    support priority inheritance mechanism
 */

#include "zk_internal.h"

static mutex_t g_mutex_pool[MUTEX_MAX_NUM];
extern task_control_block_t *volatile g_current_tcb;

/**
 * @brief Initialize mutex pool
 */
void mutex_init(void)
{
	for (zk_uint32 i = 0; i < MUTEX_MAX_NUM; i++)
	{
		g_mutex_pool[i].owner = ZK_NULL;
		g_mutex_pool[i].owner_hold_count = 0;
		g_mutex_pool[i].owner_priority = ZK_MIN_PRIORITY;
		g_mutex_pool[i].is_used = MUTEX_UNUSED;
		g_mutex_pool[i].next_mutex = ZK_NULL;
		zk_list_init(&g_mutex_pool[i].sleep_list);
	}
}

/**
 * @brief Get available mutex resource
 * @param mutex_handle Pointer to store mutex handle
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
static zk_error_code_t get_mutex_resource(zk_uint32 *mutex_handle)
{
	for (zk_uint32 i = 0; i < MUTEX_MAX_NUM; i++)
	{
		if (g_mutex_pool[i].is_used == MUTEX_UNUSED)
		{
			*mutex_handle = i;
			return ZK_SUCCESS;
		}
	}
	return ZK_ERR_RESOURCE_UNAVAILABLE;
}

/**
 * @brief Create a mutex
 * @param mutex_handle Pointer to store mutex handle
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t mutex_create(zk_uint32 *mutex_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;

	ZK_ASSERT_NULL_POINTER(mutex_handle);

	ZK_ENTER_CRITICAL();
	ret = get_mutex_resource(mutex_handle);
	if (ret != ZK_SUCCESS)
	{
		goto mutex_create_exit;
	}

	g_mutex_pool[*mutex_handle].owner_hold_count = 0;
	g_mutex_pool[*mutex_handle].owner = ZK_NULL;
	g_mutex_pool[*mutex_handle].owner_priority = ZK_MIN_PRIORITY;
	g_mutex_pool[*mutex_handle].next_mutex = ZK_NULL;
	zk_list_init(&g_mutex_pool[*mutex_handle].sleep_list);
	g_mutex_pool[*mutex_handle].is_used = MUTEX_USED;

mutex_create_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Recursively propagate priority inheritance (chain propagation)
 * @param task High priority task requesting the mutex
 * @param mutex Mutex being requested
 */
static void mutex_priority_inheritance_chain(task_control_block_t *task, mutex_t *mutex)
{
	task_control_block_t *owner = mutex->owner;
	zk_uint8 required_priority = task->priority;
	mutex_t *current_mutex = mutex;
	zk_uint8 depth = 0;
	const zk_uint8 MAX_CHAIN_DEPTH = 8;

	while (owner != ZK_NULL && depth < MAX_CHAIN_DEPTH)
	{
		if (owner->priority <= required_priority)
		{
			break;
		}

		task_change_priority_temp(owner, required_priority);
		current_mutex->owner_priority = required_priority;

		if (owner->holding_mutex != ZK_NULL)
		{
			current_mutex = owner->holding_mutex;
			owner = current_mutex->owner;
			depth++;
		}
		else
		{
			break;
		}
	}
}

/**
 * @brief Block current task on mutex
 * @param task Current task
 * @param mutex Mutex to block on
 * @param block_type Block type
 * @note P1: Uses chain priority inheritance
 */
static void mutex_sleep(task_control_block_t *task, mutex_t *mutex, block_type_t block_type)
{
	task->holding_mutex = mutex;

	if (task->priority < mutex->owner_priority)
	{
		mutex_priority_inheritance_chain(task, mutex);
	}

	task_ready_to_block(task, &mutex->sleep_list, block_type, BLOCK_SORT_PRIO);
}

/**
 * @brief Lock mutex
 * @param mutex_handle Mutex handle
 * @param block_type Block type
 * @param timeout Timeout in ticks
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
static zk_error_code_t mutex_lock_internal(zk_uint32 mutex_handle, block_type_t block_type,
										   zk_uint32 timeout)
{
	zk_error_code_t ret = ZK_SUCCESS;
	mutex_t *mutex = ZK_NULL;
	task_control_block_t *current_task = g_current_tcb;

	CHECK_MUTEX_HANDLE_VALID(mutex_handle);
	CHECK_MUTEX_CREATED(mutex_handle);

	ZK_ENTER_CRITICAL();

	if (is_scheduler_suspending())
	{
		ret = ZK_ERR_STATE;
		goto mutex_lock_exit;
	}

	mutex = MUTEX_HANDLE_TO_POINTER(mutex_handle);

	if (mutex->owner_hold_count == 0)
	{
		mutex->owner = current_task;
		mutex->owner_hold_count = 1;
		mutex->owner_priority = current_task->priority;
		mutex->next_mutex = current_task->holding_mutex;
		current_task->holding_mutex = mutex;
		goto mutex_lock_exit;
	}

	if (mutex->owner == current_task)
	{
		mutex->owner_hold_count++;
		goto mutex_lock_exit;
	}

	if (timeout == 0)
	{
		ret = ZK_ERR_FAILED;
		goto mutex_lock_exit;
	}

	current_task->event_timeout_wakeup = EVENT_NO_TIMEOUT;
	current_task->wake_up_time = get_current_time() + timeout;

	ZK_ENTER_CRITICAL();
	mutex_sleep(current_task, mutex, block_type);
	schedule();
	ZK_EXIT_CRITICAL();

	if (current_task->event_timeout_wakeup == EVENT_WAIT_TIMEOUT)
	{
		current_task->holding_mutex = ZK_NULL;
		ret = ZK_ERR_TIMEOUT;
	}
	else
	{
		// get mutex success
	}

mutex_lock_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Lock mutex with infinite timeout
 * @param mutex_handle Mutex handle
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t mutex_lock(zk_uint32 mutex_handle)
{
	return mutex_lock_internal(mutex_handle, BLOCK_TYPE_ENDLESS, 0XFF);
}

/**
 * @brief Lock mutex with timeout
 * @param mutex_handle Mutex handle
 * @param timeout Timeout in ticks
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t mutex_lock_timeout(zk_uint32 mutex_handle, zk_uint32 timeout)
{
	return mutex_lock_internal(mutex_handle, BLOCK_TYPE_TIMEOUT, timeout);
}

/**
 * @brief Try to lock mutex without blocking
 * @param mutex_handle Mutex handle
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t mutex_try_lock(zk_uint32 mutex_handle)
{
	return mutex_lock_internal(mutex_handle, BLOCK_TYPE_ENDLESS, 0);
}

/**
 * @brief Wake up task waiting on mutex
 * @param task Task to wake up
 * @param mutex Mutex to wake up from
 * @return zk_uint8 1 if reschedule needed, otherwise 0
 * @note P1: Update chain inheritance data structure
 */
static zk_uint8 mutex_wakeup(task_control_block_t *task, mutex_t *mutex)
{
	zk_uint8 need_reschedule = 0;
	task_control_block_t *wakeup_task = ZK_NULL;

	if (task->holding_mutex == mutex)
	{
		task->holding_mutex = mutex->next_mutex;
	}
	else
	{
		mutex_t *prev_mutex = task->holding_mutex;
		while (prev_mutex != ZK_NULL && prev_mutex->next_mutex != mutex)
		{
			prev_mutex = prev_mutex->next_mutex;
		}
		if (prev_mutex != ZK_NULL)
		{
			prev_mutex->next_mutex = mutex->next_mutex;
		}
	}

	if (task->base_priority != mutex->owner_priority)
	{
		task_resume_priority(task);
	}

	if (!zk_list_is_empty(&mutex->sleep_list))
	{
		need_reschedule = 1;
		wakeup_task =
			ZK_LIST_GET_FIRST_ENTRY(&mutex->sleep_list, task_control_block_t, event_sleep_list);

		wakeup_task->holding_mutex = ZK_NULL;

		task_block_to_ready(wakeup_task);

		mutex->owner = wakeup_task;
		mutex->owner_priority = wakeup_task->priority;
		mutex->owner_hold_count = 1;

		mutex->next_mutex = wakeup_task->holding_mutex;
		wakeup_task->holding_mutex = mutex;
	}
	else
	{
		mutex->owner_priority = ZK_MIN_PRIORITY;
		mutex->owner = ZK_NULL;
		mutex->next_mutex = ZK_NULL; /* P1: 清除链 */
	}

	return need_reschedule;
}

/**
 * @brief Unlock mutex
 * @param mutex_handle Mutex handle
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t mutex_unlock(zk_uint32 mutex_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	task_control_block_t *current_task = g_current_tcb;
	mutex_t *mutex = ZK_NULL;

	CHECK_MUTEX_HANDLE_VALID(mutex_handle);
	CHECK_MUTEX_CREATED(mutex_handle);

	ZK_ENTER_CRITICAL();

	if (is_scheduler_suspending())
	{
		ret = ZK_ERR_STATE;
		goto mutex_unlock_exit;
	}

	mutex = MUTEX_HANDLE_TO_POINTER(mutex_handle);

	if (mutex->owner_hold_count == 0)
	{
		ret = ZK_ERR_SYNC_NOT_OWNER;
		goto mutex_unlock_exit;
	}

	if (mutex->owner != current_task)
	{
		ret = ZK_ERR_SYNC_NOT_OWNER;
		goto mutex_unlock_exit;
	}

	mutex->owner_hold_count--;

	if (mutex->owner_hold_count != 0)
	{
		goto mutex_unlock_exit;
	}

	if (mutex_wakeup(current_task, mutex))
	{
		schedule();
	}

mutex_unlock_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Destroy mutex
 * @param mutex_handle Mutex handle
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t mutex_destroy(zk_uint32 mutex_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	mutex_t *mutex = ZK_NULL;

	CHECK_MUTEX_HANDLE_VALID(mutex_handle);
	CHECK_MUTEX_CREATED(mutex_handle);

	ZK_ENTER_CRITICAL();
	mutex = MUTEX_HANDLE_TO_POINTER(mutex_handle);

	if (!zk_list_is_empty(&mutex->sleep_list))
	{
		ret = ZK_ERR_STATE;
		goto mutex_destroy_exit;
	}

	if (mutex->owner_hold_count > 0)
	{
		ret = ZK_ERR_STATE;
		goto mutex_destroy_exit;
	}

	mutex->owner = ZK_NULL;
	mutex->is_used = MUTEX_UNUSED;
	mutex->owner_priority = ZK_MIN_PRIORITY;
	mutex->next_mutex = ZK_NULL;

mutex_destroy_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}
