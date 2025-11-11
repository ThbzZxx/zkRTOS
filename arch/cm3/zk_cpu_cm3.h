/**
 * @file    zk_cpu_cm3.h
 * @brief   ZK-RTOS Cortex-M3 架构具体实现头文件
 * @version 3.1
 * @note    实现 zk_cpu.h 中定义的抽象接口
 *          包含 Cortex-M3 特有的寄存器定义和内联函数
 *
 * ⚠️  警告: 此文件为内核代码,用户不应修改!
 *          硬件配置请修改 config/zk_config.h
 */

#ifndef ZK_CPU_CM3_H
#define ZK_CPU_CM3_H

/* ==================== 包含配置文件 ==================== */
#include "zk_config.h"  /* 包含用户配置(CPU频率、中断优先级等) */
#include "zk_def.h"
/* ==================== 前置声明：避免循环依赖 ==================== */
#ifndef ZK_DEF_H
typedef unsigned char       zk_uint8;
typedef unsigned int        zk_uint32;
#endif

/* ==================== SysTick 时钟源计算 ==================== */
#define ZK_CM3_SYSTICK_CLK_BIT       (1UL << 2UL)  /* 使用CPU时钟 */

/* ==================== Cortex-M3 寄存器定义 ==================== */
#define ZK_CM3_SYSTICK_CTRL_REG           (*((volatile zk_uint32 *)0xe000e010))
#define ZK_CM3_SYSTICK_LOAD_REG           (*((volatile zk_uint32 *)0xe000e014))
#define ZK_CM3_SYSTICK_CURRENT_VALUE_REG  (*((volatile zk_uint32 *)0xe000e018))
#define ZK_CM3_SHPR3_REG                  (*((volatile zk_uint32 *)0xe000ed20))
#define ZK_CM3_INT_CTRL_REG               (*((volatile zk_uint32 *)0xe000ed04))

/* 寄存器位定义 */
#define ZK_CM3_SYSTICK_INT_BIT        (1UL << 1UL)
#define ZK_CM3_SYSTICK_ENABLE_BIT     (1UL << 0UL)
#define ZK_CM3_PENDSV_PRI             (((zk_uint32)ZK_KERNEL_INTERRUPT_PRIORITY) << 16UL)
#define ZK_CM3_SYSTICK_PRI            (((zk_uint32)ZK_KERNEL_INTERRUPT_PRIORITY) << 24UL)
#define ZK_CM3_PENDSVSET_BIT          (1UL << 28UL)

/* 栈初始化常量 */
#define ZK_CM3_INITIAL_XPSR           (0x01000000)
#define ZK_CM3_START_ADDRESS_MASK     (0xfffffffeUL)


/* ==================== 临界区宏 ==================== */
#define ZK_ENTER_CRITICAL()     zk_cpu_enter_critical()
#define ZK_EXIT_CRITICAL()      zk_cpu_exit_critical()

/* ==================== 内联函数：BASEPRI 操作 ==================== */
#define ZK_INLINE          __inline
#define ZK_FORCE_INLINE    __inline

extern volatile zk_uint32 zk_critical_nesting;

/**
 * @brief FFS (Find First Set) 指令：查找最低位的 1
 * @param value 输入值（优先级位图）
 * @return 最低位 1 的位置（0-31），用于查找最高优先级
 * @note 实现原理：RBIT（位反转）+ CLZ（前导零计数）
 *       bit 0 = 优先级 0（最高优先级）
 */
__asm static ZK_FORCE_INLINE zk_uint8 zk_cpu_clz(zk_uint32 value)
{
    RBIT    r0, r0      /* 位反转：最低位变最高位 */
    CLZ     r0, r0      /* 计算前导零 = 原最低位 1 的位置 */
    BX      lr          /* 返回 */
}

/**
 * @brief 进入临界区（内联函数，零开销）
 * @note 使用 BASEPRI 屏蔽优先级 >= 191 的中断
 */
static ZK_FORCE_INLINE void zk_cpu_enter_critical(void)
{
    zk_uint32 basepri = ZK_MAX_SYSCALL_INTERRUPT_PRIORITY;
    __asm
    {
        msr basepri, basepri
        dsb
        isb
    }
    zk_critical_nesting++;
}


/**
 * @brief 退出临界区
 */
static ZK_FORCE_INLINE void zk_cpu_exit_critical(void)
{
    zk_critical_nesting--;
    if (zk_critical_nesting == 0)
    {
        zk_uint32 basepri_zero = 0;
        __asm
        {
            msr basepri, basepri_zero
        }
    }
}

/* ==================== 函数声明 ==================== */

/* SysTick 和中断相关（zk_cpu_cm3.c 实现）*/
void zk_cpu_systick_config(void);
zk_uint8 zk_cpu_cm3_is_in_interrupt(void);

/* 栈初始化（zk_cpu_cm3.c 实现）*/
void *zk_cpu_cm3_stack_init(zk_uint32 *stack_top,
                        zk_uint32 task_entry,
                        void *param);

void *zk_arch_prepare_stack(void *stack_start, void *param);

/* 调度器启动（zk_cpu_cm3.c 实现，调用汇编函数）*/
zk_uint32 zk_cpu_cm3_start_scheduler(void);

/* 汇编实现函数（context_rvds.s 实现）*/
void zk_asm_start_first_task(void);
void zk_asm_svc_handler(void);
void zk_asm_pendsv_handler(void);
void zk_asm_systick_handler(void);

/* 触发 PendSV */
static inline void zk_cpu_trigger_pendsv(void)
{
    ZK_CM3_INT_CTRL_REG = ZK_CM3_PENDSVSET_BIT;
}

#endif /* ZK_CPU_CM3_H */
