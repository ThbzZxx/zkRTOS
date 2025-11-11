/**
 * @file    zk_sem.c
 * @brief	semaphore management module
 */

#include "zk_internal.h"

static semaphore_t g_sem_pool[SEM_MAX_NUM];
extern task_control_block_t *volatile g_current_tcb;

#define SEM_HANDLE_TO_POINTER(handle) (&g_sem_pool[handle])

#define CHECK_SEM_HANDLE_VALID(handle)                                                             \
	if (handle >= SEM_MAX_NUM)                                                                     \
	return ZK_ERR_INVALID_HANDLE

#define CHECK_SEM_CREATED(handle)                                                                  \
	if (g_sem_pool[handle].is_used == SEM_UNUSED)                                                  \
	return ZK_ERR_STATE

void sem_init(void)
{
	for (zk_uint32 i = 0; i < SEM_MAX_NUM; i++)
	{
		g_sem_pool[i].count = 0;
		g_sem_pool[i].is_used = SEM_UNUSED;
		zk_list_init(&g_sem_pool[i].wait_list);
	}
}

static zk_error_code_t get_sem_resource(zk_uint32 *sem_handle)
{
	for (zk_uint32 i = 0; i < SEM_MAX_NUM; i++)
	{
		if (g_sem_pool[i].is_used == SEM_UNUSED)
		{
			*sem_handle = i;
			return ZK_SUCCESS;
		}
	}
	return ZK_ERR_RESOURCE_UNAVAILABLE;
}

zk_error_code_t sem_create(zk_uint32 *sem_handle, zk_uint32 initial_count)
{
	zk_error_code_t ret = ZK_SUCCESS;

	ZK_ASSERT_NULL_POINTER(sem_handle);

	if (initial_count > SEM_COUNT_MAX)
	{
		return ZK_ERR_SYNC_INVALID;
	}

	ZK_ENTER_CRITICAL();

	ret = get_sem_resource(sem_handle);
	if (ret != ZK_SUCCESS)
	{
		goto sem_create_exit;
	}

	g_sem_pool[*sem_handle].count = initial_count;
	g_sem_pool[*sem_handle].is_used = SEM_USED;
	zk_list_init(&g_sem_pool[*sem_handle].wait_list);

sem_create_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

static zk_error_code_t sem_get_internal(zk_uint32 sem_handle, block_type_t block_type,
										zk_uint32 timeout)
{
	zk_error_code_t ret = ZK_SUCCESS;
	semaphore_t *sem = ZK_NULL;
	task_control_block_t *current_task = g_current_tcb;

	CHECK_SEM_HANDLE_VALID(sem_handle);
	CHECK_SEM_CREATED(sem_handle);

	ZK_ENTER_CRITICAL();

	if (is_scheduler_suspending())
	{
		ret = ZK_ERR_STATE;
		goto sem_get_exit;
	}

	sem = SEM_HANDLE_TO_POINTER(sem_handle);

	if (sem->count > 0)
	{
		sem->count--;
		goto sem_get_exit;
	}

	if (timeout == 0)
	{
		ret = ZK_ERR_FAILED;
		goto sem_get_exit;
	}

	current_task->event_timeout_wakeup = EVENT_NO_TIMEOUT;
	current_task->wake_up_time = get_current_time() + timeout;
	task_ready_to_block(current_task, &sem->wait_list, block_type, BLOCK_SORT_PRIO);
	schedule();
	ZK_EXIT_CRITICAL();

	ZK_ENTER_CRITICAL();
	if (current_task->event_timeout_wakeup == EVENT_WAIT_TIMEOUT)
	{
		ret = ZK_ERR_TIMEOUT;
	}

sem_get_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

zk_error_code_t sem_get(zk_uint32 sem_handle)
{
	return sem_get_internal(sem_handle, BLOCK_TYPE_ENDLESS, 0XFF);
}

zk_error_code_t sem_try_get(zk_uint32 sem_handle)
{
	return sem_get_internal(sem_handle, BLOCK_TYPE_TIMEOUT, 0);
}

zk_error_code_t sem_get_timeout(zk_uint32 sem_handle, zk_uint32 timeout)
{
	return sem_get_internal(sem_handle, BLOCK_TYPE_TIMEOUT, timeout);
}

zk_error_code_t sem_release(zk_uint32 sem_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	semaphore_t *sem = ZK_NULL;
	task_control_block_t *wakeup_task = ZK_NULL;

	CHECK_SEM_HANDLE_VALID(sem_handle);
	CHECK_SEM_CREATED(sem_handle);

	ZK_ENTER_CRITICAL();
	sem = SEM_HANDLE_TO_POINTER(sem_handle);

	if (sem->count == SEM_COUNT_MAX)
	{
		ret = ZK_ERR_SYNC_INVALID;
		goto sem_release_exit;
	}

	if (!zk_list_is_empty(&sem->wait_list))
	{
		wakeup_task =
			ZK_LIST_GET_FIRST_ENTRY(&sem->wait_list, task_control_block_t, event_sleep_list);
		task_block_to_ready(wakeup_task);
		schedule();
	}
	else
	{
		sem->count++;
	}

sem_release_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

zk_error_code_t sem_destroy(zk_uint32 sem_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	semaphore_t *sem = ZK_NULL;
	task_control_block_t *wakeup_task = ZK_NULL;

	CHECK_SEM_HANDLE_VALID(sem_handle);
	CHECK_SEM_CREATED(sem_handle);

	ZK_ENTER_CRITICAL();
	sem = SEM_HANDLE_TO_POINTER(sem_handle);

	while (!zk_list_is_empty(&sem->wait_list))
	{
		wakeup_task =
			ZK_LIST_GET_FIRST_ENTRY(&sem->wait_list, task_control_block_t, event_sleep_list);
		task_block_to_ready(wakeup_task);
	}

	sem->count = 0;
	sem->is_used = SEM_UNUSED;

	schedule();
	ZK_EXIT_CRITICAL();
	return ret;
}
