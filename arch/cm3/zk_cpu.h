/**
 * @file    zk_cpu.h
 * @brief   ZK-RTOS CPU 抽象接口层定义
 * @version 2.0
 * @note    定义所有 CPU 架构必须实现的抽象接口
 *          移植到新架构时,需要在 libcpu/<arch>/ 目录下实现这些接口
 *
 * 命名规范:
 * - 抽象接口: include/zk_cpu.h (本文件)
 * - Cortex-M3实现: libcpu/cm3/zk_cpu_cm3.h/c
 * - Cortex-M4实现: libcpu/cm4/zk_cpu_cm4.h/c
 * - RISC-V实现: libcpu/riscv/zk_cpu_riscv.h/c
 */

#ifndef ZK_CPU_H
#define ZK_CPU_H

#include "zk_def.h"

/**
 * @brief CPU 移植层操作接口结构体
 * @note  每个架构的移植层都必须提供这些接口的实现
 */
typedef struct zk_cpu_ops
{
	/**
     * @brief 初始化系统时钟(SysTick)
     * @note  配置定时器以产生周期性的 Tick 中断
     */
	void (*init_systick)(void);

	/**
     * @brief 触发上下文切换
     * @note  通常通过触发软件中断实现(如 PendSV)
     */
	void (*trigger_context_switch)(void);

	/**
     * @brief 进入临界区
     * @note  禁用中断或提高中断优先级屏蔽
     */
	void (*enter_critical)(void);

	/**
     * @brief 退出临界区
     * @note  恢复中断或降低中断优先级屏蔽
     */
	void (*exit_critical)(void);

	/**
     * @brief 启动调度器(启动第一个任务)
     * @return 无返回值(不应该返回)
     */
	zk_uint32 (*start_scheduler)(void);

	/**
     * @brief 初始化任务栈
     * @param stack_top 栈顶指针
     * @param task_entry 任务入口函数地址
     * @param param 任务参数
     * @return 初始化后的栈指针
     */
	void *(*stack_init)(zk_uint32 *stack_top, zk_uint32 task_entry, void *param);

	/**
     * @brief 判断当前是否在中断上下文
     * @return 1: 在中断中, 0: 不在中断中
     */
	zk_uint8 (*is_in_interrupt)(void);

} zk_cpu_ops_t;

/**
 * @brief 全局 CPU 操作接口实例
 * @note  由各架构的具体实现文件(如 zk_cpu_cm3.c)实现并导出
 */
extern const zk_cpu_ops_t g_cpu_ops;

/*============================================================================
 *                      便捷宏定义(内核层使用)
 *============================================================================*/

#define zk_cpu_init_systick() g_cpu_ops.init_systick()
#define zk_cpu_trigger_pendsv() g_cpu_ops.trigger_context_switch()
#define zk_cpu_enter_critical() g_cpu_ops.enter_critical()
#define zk_cpu_exit_critical() g_cpu_ops.exit_critical()
#define zk_cpu_start_scheduler() g_cpu_ops.start_scheduler()
#define zk_cpu_stack_init(top, entry, param) g_cpu_ops.stack_init(top, entry, param)
#define zk_cpu_is_in_interrupt() g_cpu_ops.is_in_interrupt()

#endif /* ZK_CPU_H */
