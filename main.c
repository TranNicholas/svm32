#include "stm32l476xx.h"
#include "SysClock.h"
#include "SysTimer.h"
#include "UART.h"
#include "ds18b20.h"
#include <stdio.h>

extern volatile double currentTemperature;

int main(void)
{
	// Configure System Clock for 4MHz (with LSE calibration)
	System_Clock_Init();
	// Initialize Systick (no start)
	SysTick_Init();
	// Initialize Debug Console
	Init_USARTx(1);
	// Make-sure NVIC Priority Grouping is 0 (16 priority levels, no sub-priority)
	NVIC_SetPriorityGrouping((uint32_t) 0);
	// Set Priority DMA2 Channel6 TC level (LPUART1_TX)
	NVIC_SetPriority(DMA2_Channel6_IRQn, 1);
	// Set Priority DMA2 Channel7 TC level (LPUART1_RX)
	NVIC_SetPriority(DMA2_Channel7_IRQn, 1);
	// Enable DMA2 Channel6 TC interrupt (LPUART1_TX)
	NVIC_EnableIRQ(DMA2_Channel6_IRQn);
	// Enable DMA2 Channel7 TC interrupt (LPUART1_RX)
	NVIC_EnableIRQ(DMA2_Channel7_IRQn);
	DS18B20_GPIO_Init();
	DS18B20_LPUART1_Init();
	DS18B20_TX_DMA_Init();
	DS18B20_RX_DMA_Init();
	DS18B20_LPUART1_Enable();
	// Infinite loop
	while(1)
	{
		DS18B20_Process();
		printf("T: %f C\n", currentTemperature);
	}
}
