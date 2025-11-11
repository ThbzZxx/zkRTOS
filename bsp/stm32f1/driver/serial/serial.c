#include <stdint.h>
#include <stdio.h>
#include "stm32f10x_lib.h"
#include "zk_rtos.h" 

void vUARTInterruptHandler( void )
{
}

/*-----------------------------------------------------------*/

/* UART interrupt handler. */
void vUARTInterruptHandler( void );

/*-----------------------------------------------------------*/


void UART_Init(unsigned long ulWantedBaud)
{
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable USART1 clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE );	

	/* Configure USART1 Rx (PA10) as input floating */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init( GPIOA, &GPIO_InitStructure );
	
	/* Configure USART1 Tx (PA9) as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init( GPIOA, &GPIO_InitStructure );

	USART_InitStructure.USART_BaudRate = ulWantedBaud;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_InitStructure.USART_Clock = USART_Clock_Disable;
	USART_InitStructure.USART_CPOL = USART_CPOL_Low;
	USART_InitStructure.USART_CPHA = USART_CPHA_2Edge;
	USART_InitStructure.USART_LastBit = USART_LastBit_Disable;
	
	USART_Init( USART1, &USART_InitStructure );
	
	USART_ITConfig( USART1, USART_IT_RXNE, ENABLE );
	
	USART_Cmd( USART1, ENABLE );		
}
/*-----------------------------------------------------------*/



/*****************************************************
 * @brief   ZK-RTOS put character to UART
 * @param   c character to be printed
 * @note    redirect printf to UART
 ******************************************************/
void zk_putc(char c)
{
	while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE));
	USART_SendData(USART1, (uint8_t)c);
}



