/**
 * @file    zk_board.c
 * @brief   Board initialization and kernel startup functions
 * @note    Separated BSP layer and kernel layer initialization
 */

#include "zk_rtos.h"
#include "zk_internal.h"
#include "stm32f10x_it.h"

extern void UART_Init(unsigned long ulWantedBaud);

/**
 * @brief Initialize ZK-RTOS kernel (platform-independent)
 * @note  This function initializes all kernel components
 *        Call this after board_init() and before zk_start_scheduler()
 */
void zk_kernel_init(void)
{
	mem_init();
	scheduler_init();
#ifdef ZK_USING_MUTEX
	mutex_init();
#endif
#ifdef ZK_USING_QUEUE
	queue_init();
#endif
#ifdef ZK_USING_SEMAPHORE
	sem_init();
#endif
#ifdef ZK_USING_TIMER
	timer_init();
#endif
}

/**
 * @brief Start ZK-RTOS scheduler
 * @note  This function creates idle task and starts the first task
 *        Never returns
 */
void zk_start_scheduler(void)
{
	idle_task_create();
	start_scheduler();  // 调用 scheduler 的 start_scheduler，会设置 g_current_tcb

	for (;;)
		;
}
/**
 * @brief setup hardware clock and peripherals
 */
static void setup_hardware(void)
{
	RCC_DeInit();
	/* Enable HSE (high speed external clock). */
	RCC_HSEConfig(RCC_HSE_ON);
	/* Wait till HSE is ready. */
	while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
	{
	}
	/* 2 wait states required on the flash. */
	*((unsigned long *) 0x40022000) = 0x02;

	/* HCLK = SYSCLK */
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	/* PCLK2 = HCLK */
	RCC_PCLK2Config(RCC_HCLK_Div1);
	/* PCLK1 = HCLK/2 */
	RCC_PCLK1Config(RCC_HCLK_Div2);
	/* PLLCLK = 8MHz * 9 = 72 MHz. */
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
	/* Enable PLL. */
	RCC_PLLCmd(ENABLE);
	/* Wait till PLL is ready. */
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
	{
	}
	/* Select PLL as system clock source. */
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	/* Wait till PLL is used as system clock source. */
	while (RCC_GetSYSCLKSource() != 0x08)
	{
	}
	/* Enable GPIOA, GPIOB, GPIOC, GPIOD, GPIOE and AFIO clocks */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
							   RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO,
						   ENABLE);
	/* SPI2 Periph clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
	/* Set the Vector Table base address at 0x08000000 */
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	/* Configure HCLK clock as SysTick clock source. */
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
}

void board_init(void)
{
	setup_hardware();
	UART_Init(115200);
}

/**
 * @brief Simple blocking delay function (busy-wait)
 * @param ms Delay time in milliseconds
 * @note  This is a simple busy-wait delay, does NOT use scheduler
 *        Use this in initialization code or when scheduler is not running
 *        For tasks, use task_delay() instead
 */
void zk_delay_ms(zk_uint32 ms)
{
	/* At 72MHz, approximately 72000 cycles per millisecond */
	/* Divided by loop overhead (~3 cycles per iteration) = ~24000 iterations/ms */
	volatile zk_uint32 count = ms * 24000;
	while (count--)
	{
		/* Busy wait */
	}
}
