#include "SysTimer.h"

volatile uint32_t timer;

void SysTick_Init() {
	// Setup ticks for 1ms period
	SysTick->LOAD  = 3999UL; // set reload register
	// Set Priority for Systick Interrupt
	NVIC_SetPriority (SysTick_IRQn, 1);
	// Disable SysTick IRQ and SysTick Timer
	SysTick->CTRL &= ~(SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
}

void SysTick_Handler(void) {
	if (timer != 0) {
		timer--;
	}
}

void delay(uint32_t T) {
	timer = T;
	// Reset the SysTick Counter Value
	SysTick->VAL = 0UL;
	// Enable SysTick IRQ and SysTick Timer
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
	// Wait here for delay to elapse
	while(timer !=0);
	// Disable SysTick IRQ and SysTick Timer
	SysTick->CTRL &= ~(SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
}
