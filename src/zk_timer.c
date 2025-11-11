/**
 * @file    zk_timer.c
 * @brief   ZK-RTOS software timer management module (optimized version)
 * @note    Inspired by StaRT RTOS design:
 *          1. Check timers directly in SysTick interrupt
 *          2. Use expired_list to shorten critical section time
 *          3. Execute callback functions outside critical section
 */

#include "zk_internal.h"

static timer_manager_t g_timer_manager;
static timer_t g_timer_pool[TIMER_MAX_NUM];

#define TIMER_HANDLE_TO_POINTER(handle) (&g_timer_pool[handle])

#define CHECK_TIMER_HANDLE_VALID(handle)                                                           \
	if (handle >= TIMER_MAX_NUM)                                                                   \
	return ZK_ERR_INVALID_HANDLE

#define CHECK_TIMER_CREATED(handle)                                                                \
	if (g_timer_pool[handle].is_used == 0)                                                         \
	return ZK_ERR_STATE

/**
 * @brief Initialize timer module
 */
void timer_init(void)
{
	zk_uint32 i;
	for (i = 0; i < TIMER_MAX_NUM; i++)
	{
		g_timer_pool[i].is_used = 0;
		g_timer_pool[i].status = TIMER_STOP;
		zk_list_init(&g_timer_pool[i].list);
	}
	zk_list_init(&g_timer_manager.timers_list);
}

/**
 * @brief Get available timer resource
 */
static zk_error_code_t get_timer_resource(zk_uint32 *timer_handle)
{
	zk_uint32 i;
	for (i = 0; i < TIMER_MAX_NUM; i++)
	{
		if (g_timer_pool[i].is_used == 0)
		{
			break;
		}
	}
	if (i >= TIMER_MAX_NUM)
	{
		return ZK_ERR_RESOURCE_UNAVAILABLE;
	}
	*timer_handle = i;
	return ZK_SUCCESS;
}

/**
 * @brief Create timer
 */
zk_error_code_t timer_create(zk_uint32 *timer_handle, timer_mode_t mode, zk_uint32 interval,
							 timer_handler_t handler, void *param)
{
	zk_error_code_t ret = ZK_SUCCESS;

	ZK_ASSERT_NULL_POINTER(timer_handle);
	ZK_ASSERT_NULL_POINTER(handler);

	if (interval == 0 || interval >= ZK_TSK_DLY_MAX)
	{
		return ZK_ERR_OUT_OF_RANGE;
	}

	ZK_ENTER_CRITICAL();

	ret = get_timer_resource(timer_handle);
	if (ret != ZK_SUCCESS)
	{
		goto timer_create_exit;
	}

	g_timer_pool[*timer_handle].interval = interval;
	g_timer_pool[*timer_handle].mode = mode;
	g_timer_pool[*timer_handle].param = param;
	g_timer_pool[*timer_handle].handler = handler;
	g_timer_pool[*timer_handle].wake_up_time = 0;
	g_timer_pool[*timer_handle].status = TIMER_STOP;
	zk_list_init(&g_timer_pool[*timer_handle].list);
	g_timer_pool[*timer_handle].is_used = 1;

timer_create_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Insert timer into sorted timer list (ordered by timeout time)
 * @param timer Timer to insert
 * @note Called within critical section
 */
static void add_timer_to_list(timer_t *timer)
{
	zk_list_node_t *target_list = &g_timer_manager.timers_list;
	zk_list_node_t *iterator = ZK_NULL;
	timer_t *timer_iterator = ZK_NULL;

	if (zk_list_is_empty(target_list))
	{
		zk_list_add_after(&timer->list, target_list);
		return;
	}

	ZK_LIST_FOR_EACH_NODE(iterator, target_list)
	{
		timer_iterator = ZK_LIST_GET_OWNER(iterator, timer_t, list);
		if ((zk_int32) (timer_iterator->wake_up_time - timer->wake_up_time) > 0)
		{
			break;
		}
	}

	if (iterator == target_list)
	{
		zk_list_add_before(&timer->list, target_list);
	}
	else
	{
		zk_list_add_before(&timer->list, iterator);
	}
}

/**
 * @brief Remove timer from timer list
 * @param timer Timer to remove
 * @note Called within critical section
 */
static void remove_timer_from_list(timer_t *timer)
{
	zk_list_delete(&timer->list);
}

/**
 * @brief Start timer
 */
zk_error_code_t timer_start(zk_uint32 timer_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	timer_t *timer = ZK_NULL;

	CHECK_TIMER_HANDLE_VALID(timer_handle);
	CHECK_TIMER_CREATED(timer_handle);

	ZK_ENTER_CRITICAL();

	timer = TIMER_HANDLE_TO_POINTER(timer_handle);

	if (timer->status == TIMER_RUNNING)
	{
		remove_timer_from_list(timer);
	}

	timer->wake_up_time = get_current_time() + timer->interval;
	timer->status = TIMER_RUNNING;

	add_timer_to_list(timer);

	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Stop timer
 */
zk_error_code_t timer_stop(zk_uint32 timer_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	timer_t *timer = ZK_NULL;

	CHECK_TIMER_HANDLE_VALID(timer_handle);
	CHECK_TIMER_CREATED(timer_handle);

	ZK_ENTER_CRITICAL();

	timer = TIMER_HANDLE_TO_POINTER(timer_handle);

	if (timer->status == TIMER_STOP)
	{
		ret = ZK_ERR_STATE;
		goto timer_stop_exit;
	}

	remove_timer_from_list(timer);
	timer->status = TIMER_STOP;

timer_stop_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Delete timer
 */
zk_error_code_t timer_delete(zk_uint32 timer_handle)
{
	zk_error_code_t ret = ZK_SUCCESS;
	timer_t *timer = ZK_NULL;

	CHECK_TIMER_HANDLE_VALID(timer_handle);
	CHECK_TIMER_CREATED(timer_handle);

	ZK_ENTER_CRITICAL();

	timer = TIMER_HANDLE_TO_POINTER(timer_handle);

	/* 如果定时器正在运行，先停止 */
	if (timer->status == TIMER_RUNNING)
	{
		remove_timer_from_list(timer);
		timer->status = TIMER_STOP;
	}

	timer->is_used = 0;

	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Check and process expired timers (called in SysTick interrupt)
 * @param current_time Current system time
 * @note Optimization: use temporary expired_list to shorten critical section time
 */
void timer_check(zk_uint32 current_time)
{
	zk_list_node_t expired_list;
	zk_list_node_t *iterator;
	timer_t *timer;

	zk_list_init(&expired_list);

	ZK_ENTER_CRITICAL();

	while (!zk_list_is_empty(&g_timer_manager.timers_list))
	{
		timer = ZK_LIST_GET_FIRST_ENTRY(&g_timer_manager.timers_list, timer_t, list);

		if ((zk_int32) (current_time - timer->wake_up_time) >= 0)
		{
			remove_timer_from_list(timer);

			zk_list_add_before(&timer->list, &expired_list);
		}
		else
		{
			break;
		}
	}

	ZK_EXIT_CRITICAL();

	while (!zk_list_is_empty(&expired_list))
	{
		iterator = expired_list.next;
		timer = ZK_LIST_GET_OWNER(iterator, timer_t, list);

		zk_list_delete(iterator);

		if (timer->handler != ZK_NULL)
		{
			timer->handler(timer->param);
		}

		ZK_ENTER_CRITICAL();
		if (timer->mode == TIMER_AUTO_RELOAD)
		{
			timer->wake_up_time = get_current_time() + timer->interval;
			add_timer_to_list(timer);
			timer->status = TIMER_RUNNING;
		}
		else
		{
			timer->status = TIMER_STOP;
		}
		ZK_EXIT_CRITICAL();
	}
}

/**
 * @brief Reset timer (modify interval time)
 * @param timer_handle Timer handle
 * @param new_interval New interval time in ticks
 * @return Error code
 */
zk_error_code_t timer_reset(zk_uint32 timer_handle, zk_uint32 new_interval)
{
	zk_error_code_t ret = ZK_SUCCESS;
	timer_t *timer = ZK_NULL;
	zk_bool was_running = ZK_FALSE;

	CHECK_TIMER_HANDLE_VALID(timer_handle);
	CHECK_TIMER_CREATED(timer_handle);

	if (new_interval == 0 || new_interval >= ZK_TSK_DLY_MAX)
	{
		return ZK_ERR_OUT_OF_RANGE;
	}

	ZK_ENTER_CRITICAL();

	timer = TIMER_HANDLE_TO_POINTER(timer_handle);
	was_running = (timer->status == TIMER_RUNNING);

	if (was_running)
	{
		remove_timer_from_list(timer);
	}

	timer->interval = new_interval;

	if (was_running)
	{
		timer->wake_up_time = get_current_time() + timer->interval;
		add_timer_to_list(timer);
		timer->status = TIMER_RUNNING;
	}

	ZK_EXIT_CRITICAL();
	return ret;
}

/**
 * @brief Get timer remaining time
 * @param timer_handle Timer handle
 * @param remaining Output parameter, remaining time in ticks
 * @return Error code
 */
zk_error_code_t timer_get_remaining(zk_uint32 timer_handle, zk_uint32 *remaining)
{
	zk_error_code_t ret = ZK_SUCCESS;
	timer_t *timer = ZK_NULL;
	zk_uint32 current_time;

	CHECK_TIMER_HANDLE_VALID(timer_handle);
	CHECK_TIMER_CREATED(timer_handle);
	ZK_ASSERT_NULL_POINTER(remaining);

	ZK_ENTER_CRITICAL();

	timer = TIMER_HANDLE_TO_POINTER(timer_handle);

	if (timer->status != TIMER_RUNNING)
	{
		*remaining = 0;
		ret = ZK_ERR_STATE;
		goto timer_get_remaining_exit;
	}

	current_time = get_current_time();

	if ((zk_int32) (timer->wake_up_time - current_time) > 0)
	{
		*remaining = timer->wake_up_time - current_time;
	}
	else
	{
		*remaining = 0;
	}

timer_get_remaining_exit:
	ZK_EXIT_CRITICAL();
	return ret;
}
