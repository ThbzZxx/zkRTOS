#ifndef __DRIVER_USART_H
#define __DRIVER_USART_H

//#include "stm32f1xx_hal.h"


#define USARTx                  USART1
#define USARTx_TX_PIN           GPIO_PIN_9
#define USARTx_RX_PIN           GPIO_PIN_10
#define USARTx_PORT             GPIOA
#define USARTx_GPIO_CLK_EN()    __HAL_RCC_GPIOA_CLK_ENABLE()
#define USARTx_CLK_EN()         __HAL_RCC_USART1_CLK_ENABLE()
#define USARTx_CLK_DIS()        __HAL_RCC_USART1_CLK_DISABLE()

extern UART_HandleTypeDef husart;


extern void UsartInit(uint32_t baudrate);
#endif
