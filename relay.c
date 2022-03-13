#include "relay.h"
#include <stdbool.h>

void Relay_Init(void) {
	// Enable GPIO Clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	
	// Initialize Relay
	GPIOA->MODER |= GPIO_MODER_MODE13_0;
	GPIOA->MODER &= ~GPIO_MODER_MODE13_1;
	GPIOA->OTYPER &= ~GPIO_OTYPER_OT13;
	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD13;
}

void Relay_Off(void) {
	GPIOA->ODR &= ~GPIO_ODR_OD13;
}

void Relay_On(void) {
	GPIOA->ODR |= GPIO_ODR_OD13;
}
