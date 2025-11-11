/**
 * @file    zk_time.c
 * @brief   ZK-RTOS system time management module
 */

#include "zk_internal.h"

static volatile zk_uint32 g_current_time = 0;
static volatile zk_uint32 g_total_run_time = 0;

void zk_time_init(void)
{
	g_current_time = CONFIG_TICK_COUNT_INIT_VALUE;
	g_total_run_time = 0;
}

void increment_time(void)
{
	g_current_time++;
	g_total_run_time++;
}

zk_uint32 get_current_time(void)
{
	return g_current_time;
}

/**
 * @brief   Get total system runtime
 * @return  Total system runtime in ticks
 */
zk_uint32 get_total_run_time(void)
{
	return g_total_run_time;
}
