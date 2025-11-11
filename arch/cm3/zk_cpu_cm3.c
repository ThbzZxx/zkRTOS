/**
 * @file    zk_cpu_cm3.c
 * @brief   ZK-RTOS Cortex-M3 architecture-specific implementation
 * @version 2.0
 * @note    Implements abstract interfaces defined in zk_cpu.h
 *          Includes stack initialization, critical section management, SysTick configuration, etc.
 */

#include "zk_cpu_cm3.h"
#include "zk_internal.h"
#include "zk_cpu.h"  /* 包含 zk_cpu_ops_t 类型定义 */

/* ==================== Static Function Declarations ==================== */
static void zk_task_exit_error(void);

/* ==================== Static Variables ==================== */
volatile zk_uint32 zk_critical_nesting = 0xaaaaaaaa;

/* ==================== Stack Initialization ==================== */

/**
 * @brief Stack frame structure (must match hardware/software stack push order!)
 * @note Stack grows from high to low address, structure layout is from low to high address
 */
typedef struct
{
	/* 软件保存的寄存器 R4-R11（在栈的低地址端）*/
	zk_uint32 r4;
	zk_uint32 r5;
	zk_uint32 r6;
	zk_uint32 r7;
	zk_uint32 r8;
	zk_uint32 r9;
	zk_uint32 r10;
	zk_uint32 r11;

	/* 硬件自动保存的寄存器（异常入口时，在栈的高地址端）*/
	zk_uint32 r0;
	zk_uint32 r1;
	zk_uint32 r2;
	zk_uint32 r3;
	zk_uint32 r12;
	zk_uint32 lr;
	zk_uint32 pc;
	zk_uint32 psr;
} zk_stack_frame_t;

/**
 * @brief Initialize task stack frame
 * @param stack_top Stack top pointer (high address)
 * @param task_entry Task entry function
 * @param param Task parameter
 * @return Initialized stack pointer (pointing to R4 position)
 */
void *zk_cpu_cm3_stack_init(zk_uint32 *stack_top, zk_uint32 task_entry, void *param)
{
	zk_stack_frame_t *frame;

	/* 8字节对齐（ARM EABI 要求）*/
	stack_top = (zk_uint32 *) (((zk_uint32) stack_top) & ~(0x07UL));

	/* 从栈顶向下分配栈帧空间 */
	stack_top = (zk_uint32 *) ((zk_uint8 *) stack_top - sizeof(zk_stack_frame_t));
	frame = (zk_stack_frame_t *) stack_top;

	/* 清零整个栈帧（可选，用于调试）*/
	zk_uint32 i;
	for (i = 0; i < (sizeof(zk_stack_frame_t) / sizeof(zk_uint32)); i++)
	{
		((zk_uint32 *) frame)[i] = 0;
	}

	/* 初始化硬件栈帧（必须初始化的字段）*/
	frame->psr = ZK_CM3_INITIAL_XPSR;					/* xPSR: Thumb位 */
	frame->pc = task_entry & ZK_CM3_START_ADDRESS_MASK; /* PC: 任务入口 */
	frame->lr = (zk_uint32) zk_task_exit_error;			/* LR: 错误退出函数 */
	frame->r0 = (zk_uint32) param;						/* R0: 任务参数 */

	/* 返回栈指针（指向 R4，即软件栈帧的起始位置）*/
	return stack_top;
}

/**
 * @brief Prepare task stack (extract information from task_init_parameter_t)
 */
void *zk_arch_prepare_stack(void *stack_start, void *param)
{
	task_init_parameter_t *task_param = (task_init_parameter_t *) param;
	zk_uint32 *stack_top =
		(zk_uint32 *) ((zk_uint32) stack_start + task_param->stack_size - sizeof(zk_uint32));
	return zk_cpu_cm3_stack_init(stack_top, (zk_uint32) (task_param->task_entry),
							 task_param->private_data);
}

/* ==================== Scheduler Startup ==================== */

/**
 * @brief Start the scheduler
 */
zk_uint32 zk_cpu_cm3_start_scheduler(void)
{
	/* 设置 PendSV 和 SysTick 中断优先级最低 */
	ZK_CM3_SHPR3_REG |= ZK_CM3_PENDSV_PRI;
	ZK_CM3_SHPR3_REG |= ZK_CM3_SYSTICK_PRI;

	/* 配置 SysTick 中断 */
	zk_cpu_systick_config();
	zk_critical_nesting = 0;

	/* 启动第一个任务（汇编实现）*/
	zk_asm_start_first_task();

	return 0;
}


/* ==================== Task Exit Error Handling ==================== */

static void zk_task_exit_error(void)
{
	/* 任务退出错误：永久禁用所有中断 */
	__asm
	{
        cpsid i /* 全局关中断 */
	}
	for (;;)
	{
	}
}

/* ==================== SysTick Interrupt Handling ==================== */

/**
 * @brief Configure SysTick timer
 */
void zk_cpu_systick_config(void)
{
	/* 停止并清除SysTick */
	ZK_CM3_SYSTICK_CTRL_REG = 0UL;
	ZK_CM3_SYSTICK_CURRENT_VALUE_REG = 0UL;

	/* 配置1ms中断 */
	ZK_CM3_SYSTICK_LOAD_REG = (ZK_SYSTICK_CLOCK_HZ / ZK_TICK_RATE_HZ) - 1UL;

	/* 选择SysTick时钟源、启用SysTick中断、启动SysTick计数器 */
	ZK_CM3_SYSTICK_CTRL_REG =
		(ZK_CM3_SYSTICK_CLK_BIT | ZK_CM3_SYSTICK_INT_BIT | ZK_CM3_SYSTICK_ENABLE_BIT);
}

/* ==================== Other Utility Functions ==================== */

/**
 * @brief Check if in interrupt context
 */
zk_uint8 zk_cpu_cm3_is_in_interrupt(void)
{
	return 0;
}

/* ==================== CPU Abstract Interface Implementation ==================== */

/**
 * @brief Cortex-M3 CPU operations interface implementation
 * @note  Implements abstract interfaces defined in zk_cpu_port.h
 */
const zk_cpu_ops_t g_cpu_ops = {
    .init_systick = zk_cpu_systick_config,
    .trigger_context_switch = zk_cpu_trigger_pendsv,
    .enter_critical = zk_cpu_enter_critical,
    .exit_critical = zk_cpu_exit_critical,
    .start_scheduler = zk_cpu_cm3_start_scheduler,
    .stack_init = zk_cpu_cm3_stack_init,
    .is_in_interrupt = zk_cpu_cm3_is_in_interrupt,
};
