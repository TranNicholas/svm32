#include "UART.h"
void Init_USARTx(int x) {
	if(x == 1) {
		UART1_Init();
		UART1_GPIO_Init();
		USART_Init(USART1);
	} else if(x == 2) {
		UART2_Init();
		UART2_GPIO_Init();
		USART_Init(USART2);
	}
}
void UART1_Init(void) {
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	RCC->CCIPR |= RCC_CCIPR_USART1SEL_0;
	RCC->CCIPR &= ~RCC_CCIPR_USART1SEL_1;
}

void UART1_GPIO_Init(void) {
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	// alternate function mode
	GPIOB->MODER &= ~(GPIO_MODER_MODE6_0 | GPIO_MODER_MODE7_0);
	GPIOB->MODER |= GPIO_MODER_MODE6_1 | GPIO_MODER_MODE7_1;
	// very high speed
	GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7;
	// push-pull output type
	GPIOB->OTYPER &= ~GPIO_OTYPER_OT6 & ~GPIO_OTYPER_OT7;
	// pull-up resistors
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR7_0;
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR6_1 & ~GPIO_PUPDR_PUPDR7_1;
	// selct AF 7 for PB6 and PB7
	GPIOB->AFR[0] |= GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7;
	GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL6_3 | GPIO_AFRL_AFSEL7_3);
}

void UART2_Init(void) {
	RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
	RCC->CCIPR |= RCC_CCIPR_USART2SEL_0;
	RCC->CCIPR &= ~RCC_CCIPR_USART2SEL_1;
}

void UART2_GPIO_Init(void) {
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	// alternate function mode
	GPIOA->MODER &= ~(GPIO_MODER_MODE2_0 | GPIO_MODER_MODE3_0);
	GPIOA->MODER |= GPIO_MODER_MODE2_1 | GPIO_MODER_MODE3_1;
	// very high speed
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR2 | GPIO_OSPEEDER_OSPEEDR3;
	// push-pull output type
	GPIOA->OTYPER &= ~(GPIO_OTYPER_OT2 | GPIO_OTYPER_OT3);
	// pull-up resistors
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR2_0 | GPIO_PUPDR_PUPDR3_0;
	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2_1 | ~GPIO_PUPDR_PUPDR3_1);
	// selct AF 7 for PA2 and PA3
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
	GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2_3 | GPIO_AFRL_AFSEL3_3);
}

void USART_Init(USART_TypeDef* USARTx) {
	// set word length to 8 bits
	// oversample by 16
	USARTx->CR1 &= ~USART_CR1_M & ~USART_CR1_OVER8; 
	USARTx->CR2 &= ~USART_CR2_STOP; // 1 stop bit
	USARTx->BRR = 417U; // set baud rate to 9600
	// USARTx->BRR = 35U; // set baud rate to 115200
	// enable transmitter and receiver
	USARTx->CR1 |= USART_CR1_TE | USART_CR1_RE; 
	USARTx->CR1 |= USART_CR1_UE; // USART enable

}

uint8_t USART_Read (USART_TypeDef * USARTx) {
	// SR_RXNE (Read data register not empty) bit is set by hardware
	while (!(USARTx->ISR & USART_ISR_RXNE));  // Wait until RXNE (RX not empty) bit is set
	// USART resets the RXNE flag automatically after reading DR
	return ((uint8_t)(USARTx->RDR & 0xFF));
	// Reading USART_DR automatically clears the RXNE flag 
}

void USART_Write(USART_TypeDef * USARTx, uint8_t *buffer, uint32_t nBytes) {
	int i;
	// TXE is cleared by a write to the USART_DR register.
	// TXE is set by hardware when the content of the TDR 
	// register has been transferred into the shift register.
	for (i = 0; i < nBytes; i++) {
		while (!(USARTx->ISR & USART_ISR_TXE));   	// wait until TXE (TX empty) bit is set
		// Writing USART_DR automatically clears the TXE flag 	
		USARTx->TDR = buffer[i] & 0xFF;
		USART_Delay(300);
	}
	while (!(USARTx->ISR & USART_ISR_TC));   		  // wait until TC bit is set
	USARTx->ISR &= ~USART_ISR_TC;
}   

void USART_Delay(uint32_t us) {
	uint32_t time = 20*us/7;    
	while(--time);
}
