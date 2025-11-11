/**
 * @file    zk_queue.c
 * @brief   queue management module
 */

#include "zk_internal.h"
extern task_control_block_t *volatile g_current_tcb;

static queue_t g_queue_pool[QUEUE_MAX_NUM];
static zk_uint32 queue_remaining_space(zk_uint32 queue_handle);

/**
 * @brief queue pool init
 */
void queue_init(void)
{
	for (zk_uint8 i = 0; i < QUEUE_MAX_NUM; i++)
	{
		g_queue_pool[i].data_buffer = ZK_NULL;
		g_queue_pool[i].element_num = 0;
		g_queue_pool[i].element_single_size = 0;
		g_queue_pool[i].is_used = QUEUE_UNUSED;
		g_queue_pool[i].read_pos = 0;
		g_queue_pool[i].write_pos = 0;
		zk_list_init(&g_queue_pool[i].reader_sleep_list);
		zk_list_init(&g_queue_pool[i].writer_sleep_list);
	}
}
/**
 * @brief get queue resource
 * @param queue_handle queue handle
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
static zk_error_code_t get_queue_resource(zk_uint32 *queue_handle)
{
	for (zk_uint32 i = 0; i < QUEUE_MAX_NUM; i++)
	{
		if (g_queue_pool[i].is_used == QUEUE_UNUSED)
		{
			*queue_handle = i;
			return ZK_SUCCESS;
		}
	}
	return ZK_ERR_RESOURCE_UNAVAILABLE;
}

/**
 * @brief create queue
 * @param queue_handle queue handle
 * @param element_single_size element single size
 * @param element_num element number
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_create(zk_uint32 *queue_handle, zk_uint32 element_single_size,
							 zk_uint32 element_num)
{
	zk_error_code_t ret = ZK_SUCCESS;
	void *data_buffer = ZK_NULL;
	zk_uint32 temp_handle = 0;

	ZK_ASSERT_NULL_POINTER(queue_handle);

	if (element_single_size == 0 || element_num == 0)
	{
		return ZK_ERR_INVALID_PARAM;
	}

	data_buffer = mem_alloc(element_num * element_single_size);
	if (data_buffer == ZK_NULL)
	{
		return ZK_ERR_NOT_ENOUGH_MEMORY;
	}

	ZK_ENTER_CRITICAL();

	ret = get_queue_resource(&temp_handle);
	if (ret != ZK_SUCCESS)
	{
		ZK_EXIT_CRITICAL();
		mem_free(data_buffer);
		return ret;
	}

	g_queue_pool[temp_handle].data_buffer = data_buffer;
	g_queue_pool[temp_handle].element_num = element_num;
	g_queue_pool[temp_handle].element_single_size = element_single_size;
	g_queue_pool[temp_handle].read_pos = 0;
	g_queue_pool[temp_handle].write_pos = 0;
	g_queue_pool[temp_handle].is_used = QUEUE_USED;

	*queue_handle = temp_handle;

	ZK_EXIT_CRITICAL();
	return ZK_SUCCESS;
}

/**
 * @brief queue sleep
 * @param tcb task control block
 * @param sleep_list_head sleep list head
 * @param block_type block type
 */
static void queue_sleep(task_control_block_t *tcb, zk_list_node_t *sleep_list_head,
						block_type_t block_type)
{
	task_ready_to_block(tcb, sleep_list_head, block_type, BLOCK_SORT_PRIO);
}

/**
 * @brief queue wakeup
 * @param sleep_list_head sleep list head
 */
static void queue_wakeup(zk_list_node_t *sleep_list_head)
{
	task_control_block_t *wake_up_tcb =
		ZK_LIST_GET_FIRST_ENTRY(sleep_list_head, task_control_block_t, event_sleep_list);
	task_block_to_ready(wake_up_tcb);
}

/**
 * @brief queue write position increase
 * @param queue queue
 */
static inline void queue_write_pos_increase(queue_t *queue)
{
	queue->write_pos++;
	if (queue->write_pos == queue->element_num)
	{
		queue->write_pos = 0;
	}
}

/**
 * @brief queue full
 * @param queue_handle queue handle
 * @return zk_uint8 ZK_TRUE if full, otherwise ZK_FALSE
 */
zk_uint8 queue_full(zk_uint32 queue_handle)
{
	return queue_remaining_space(queue_handle) == 0;
}

/**
 * @brief queue write internal
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @param block_type block type
 * @param timeout timeout
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
static zk_error_code_t queue_write_internal(zk_uint32 queue_handle, const void *buffer,
											zk_uint32 size, block_type_t block_type,
											zk_uint32 timeout)
{
	zk_error_code_t ret = ZK_SUCCESS;
	queue_t *queue = ZK_NULL;
	task_control_block_t *current_tcb = g_current_tcb;
	zk_uint32 index = 0;
	zk_uint8 *buffer_addr = ZK_NULL;

	ZK_ASSERT_NULL_POINTER(buffer);
	QUEUE_CHECK_HANDLE_VALID(queue_handle);
	QUEUE_CHECK_HANDLE_CREATED(queue_handle);

	ZK_ENTER_CRITICAL();
	queue = QUEUE_HANDLE_TO_POINTER(queue_handle);

	if (size > queue->element_single_size)
	{
		ret = ZK_ERR_QUEUE_SIZE_MISMATCH;
		goto queue_write_exit;
	}

	while (queue_full(queue_handle))
	{
		if (timeout == 0)
		{
			ret = ZK_ERR_FAILED;
			goto queue_write_exit;
		}
		if (is_scheduler_suspending())
		{
			ret = ZK_ERR_STATE;
			goto queue_write_exit;
		}

		current_tcb->wake_up_time = get_current_time() + timeout;
		current_tcb->event_timeout_wakeup = EVENT_NO_TIMEOUT;
		queue_sleep(current_tcb, &queue->writer_sleep_list, block_type);
		schedule();
		ZK_EXIT_CRITICAL();

		ZK_ENTER_CRITICAL();
		if (current_tcb->event_timeout_wakeup == EVENT_WAIT_TIMEOUT)
		{
			ret = ZK_ERR_TIMEOUT;
			goto queue_write_exit;
		}
	}

	index = queue->write_pos;
	buffer_addr = QUEUE_INDEX_TO_BUFFERADDR(queue, index);
	zk_memcpy(buffer_addr, buffer, size);
	queue_write_pos_increase(queue);

	if (!zk_list_is_empty(&queue->reader_sleep_list))
	{
		queue_wakeup(&queue->reader_sleep_list);
		schedule();
	}

queue_write_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief queue write
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @return zk_error_code_t ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_write(zk_uint32 queue_handle, const void *buffer, zk_uint32 size)
{
	ZK_CHECK_HANDLE_VALID(queue_handle, QUEUE_MAX_NUM);
	ZK_CHECK_PARAM_NOT_NULL(buffer);

	if (size == 0)
	{
		return ZK_ERR_INVALID_PARAM;
	}

	return queue_write_internal(queue_handle, buffer, size, BLOCK_TYPE_ENDLESS,
								ZK_TIMEOUT_INFINITE);
}

/**
 * @brief queue try write
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_try_write(zk_uint32 queue_handle, const void *buffer, zk_uint32 size)
{
	return queue_write_internal(queue_handle, buffer, size, BLOCK_TYPE_ENDLESS, ZK_TIMEOUT_NONE);
}

/**
 * @brief queue write timeout
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @param timeout timeout
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_write_timeout(zk_uint32 queue_handle, const void *buffer, zk_uint32 size,
									zk_uint32 timeout)
{
	return queue_write_internal(queue_handle, buffer, size, BLOCK_TYPE_TIMEOUT, timeout);
}

/**
 * @brief queue remaining space
 * @param queue_handle queue handle
 * @return zk_uint32 remaining space
 */
zk_uint32 queue_remaining_space(zk_uint32 queue_handle)
{
	queue_t *queue = QUEUE_HANDLE_TO_POINTER(queue_handle);
	zk_uint32 used_size = 0;

	if (queue->read_pos > queue->write_pos)
	{
		used_size = queue->element_num - (queue->read_pos - queue->write_pos);
	}
	else
	{
		used_size = queue->write_pos - queue->read_pos;
	}

	return queue->element_num - used_size;
}
/**
 * @brief queue empty
 * @param queue_handle queue handle
 * @return zk_uint8 ZK_TRUE if empty, otherwise ZK_FALSE
 */
zk_uint8 queue_empty(zk_uint32 queue_handle)
{
	queue_t *queue = QUEUE_HANDLE_TO_POINTER(queue_handle);
	return queue->read_pos == queue->write_pos;
}

/**
 * @brief queue read
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
static inline void queue_read_pos_increase(queue_t *queue)
{
	queue->read_pos++;
	if (queue->read_pos == queue->element_num)
	{
		queue->read_pos = 0;
	}
}

/**
 * @brief queue read internal
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @param block_type block type
 * @param timeout timeout
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
static zk_error_code_t queue_read_internal(zk_uint32 queue_handle, void *buffer, zk_uint32 size,
										   block_type_t block_type, zk_uint32 timeout)
{
	zk_error_code_t ret = ZK_SUCCESS;
	queue_t *queue = ZK_NULL;
	task_control_block_t *current_tcb = g_current_tcb;
	zk_uint32 index = 0;
	zk_uint8 *buffer_addr = ZK_NULL;

	ZK_ASSERT_NULL_POINTER(buffer);
	QUEUE_CHECK_HANDLE_VALID(queue_handle);
	QUEUE_CHECK_HANDLE_CREATED(queue_handle);

	ZK_ENTER_CRITICAL();
	queue = QUEUE_HANDLE_TO_POINTER(queue_handle);

	if (size > queue->element_single_size)
	{
		ret = ZK_ERR_QUEUE_SIZE_MISMATCH;
		goto queue_read_exit;
	}

	while (queue_empty(queue_handle))
	{
		if (timeout == 0)
		{
			ret = ZK_ERR_FAILED;
			goto queue_read_exit;
		}

		if (is_scheduler_suspending())
		{
			ret = ZK_ERR_STATE;
			goto queue_read_exit;
		}

		current_tcb->event_timeout_wakeup = EVENT_NO_TIMEOUT;
		current_tcb->wake_up_time = get_current_time() + timeout;
		queue_sleep(current_tcb, &queue->reader_sleep_list, block_type);
		schedule();
		ZK_EXIT_CRITICAL();

		ZK_ENTER_CRITICAL();
		if (current_tcb->event_timeout_wakeup == EVENT_WAIT_TIMEOUT)
		{
			ret = ZK_ERR_TIMEOUT;
			goto queue_read_exit;
		}
	}

	index = queue->read_pos;
	buffer_addr = QUEUE_INDEX_TO_BUFFERADDR(queue, index);
	zk_memcpy(buffer, buffer_addr, size);
	queue_read_pos_increase(queue);

	if (!zk_list_is_empty(&queue->writer_sleep_list))
	{
		queue_wakeup(&queue->writer_sleep_list);
		schedule();
	}

queue_read_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief queue read
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_read(zk_uint32 queue_handle, void *buffer, zk_uint32 size)
{
	return queue_read_internal(queue_handle, buffer, size, BLOCK_TYPE_ENDLESS, ZK_TIMEOUT_INFINITE);
}
/**
 * @brief queue try read
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_try_read(zk_uint32 queue_handle, void *buffer, zk_uint32 size)
{
	return queue_read_internal(queue_handle, buffer, size, BLOCK_TYPE_ENDLESS, ZK_TIMEOUT_NONE);
}
/**
 * @brief queue read timeout
 * @param queue_handle queue handle
 * @param buffer buffer
 * @param size size
 * @param timeout timeout
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_read_timeout(zk_uint32 queue_handle, void *buffer, zk_uint32 size,
								   zk_uint32 timeout)
{
	return queue_read_internal(queue_handle, buffer, size, BLOCK_TYPE_TIMEOUT, timeout);
}
/**
 * @brief queue destroy
 * @param queue_handle queue handle
 * @return zk_uint32 ZK_SUCCESS if success, otherwise error code
 */
zk_error_code_t queue_destroy(zk_uint32 queue_handle)
{
	queue_t *queue = ZK_NULL;
	zk_error_code_t ret = ZK_SUCCESS;

	QUEUE_CHECK_HANDLE_CREATED(queue_handle);
	QUEUE_CHECK_HANDLE_VALID(queue_handle);

	ZK_ENTER_CRITICAL();
	queue = QUEUE_HANDLE_TO_POINTER(queue_handle);

	if (!zk_list_is_empty(&queue->reader_sleep_list))
	{
		ret = ZK_ERR_STATE;
		goto queue_destroy_exit;
	}

	if (!zk_list_is_empty(&queue->writer_sleep_list))
	{
		ret = ZK_ERR_STATE;
		goto queue_destroy_exit;
	}

	if (!queue_empty(queue_handle))
	{
		ret = ZK_ERR_STATE;
		goto queue_destroy_exit;
	}

	mem_free(queue->data_buffer);
	queue->is_used = QUEUE_UNUSED;

queue_destroy_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}
