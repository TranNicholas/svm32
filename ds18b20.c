#include "ds18b20.h"
#include "SysTimer.h"
#include <stdio.h>
#include <stddef.h>
// Heavily based off of nucleo-64_L476_DS18B20
// https://gitlab.polytech.umontpellier.fr/gauthier.chabrolin/nucleo-64_l476_ds18b20
// Specifically the file bsp/src/ds18b20.c
#define BIT_0 ((uint8_t) 0x00U)
#define BIT_1 ((uint8_t) 0xFFU)
#define RESET_PULSE ((uint8_t) 0xF0U)

// Temperature convert, {Skip ROM = 0xCC, Convert = 0x44}
static const uint8_t temp_convert[] =
{
	BIT_0, BIT_0, BIT_1, BIT_1, BIT_0, BIT_0, BIT_1, BIT_1,
	BIT_0, BIT_0, BIT_1, BIT_0, BIT_0, BIT_0, BIT_1, BIT_0
};

// Temperature data read, {Skip ROM = 0xCC, Scratch read = 0xBE}
static const uint8_t temp_read[] =
{
	BIT_0, BIT_0, BIT_1, BIT_1, BIT_0, BIT_0, BIT_1, BIT_1, //0xCC 1100 1100
	BIT_0, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_0, BIT_1, //0xBE 1011 1110
	BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, //0xFF
	BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1, BIT_1  //0xFF
};

static uint8_t temperatureData[sizeof(temp_read)]; // Received temperature data using DMA

static uint8_t temperatureDataReceived = 0; // Temperature data received flag

volatile double currentTemperature = 0; // Current temperature in degrees Celsius

void DS18B20_CMDTransmit(const uint8_t * cmd, uint8_t size)
{
	if (cmd != NULL)
	{
		/*
		 *  Wait until DMA2 channel 6 is disabled
		 *  The enable flag shall reset when DMA transfer complete
		 */
		while((DMA_CCR_EN & DMA2_Channel6->CCR) == DMA_CCR_EN);
		
		// Memory buffer address
		DMA2_Channel6->CMAR = (uint32_t)cmd;
		
		// Number of data to be transfered
		DMA2_Channel6->CNDTR = (uint32_t)size;
		
		// Clear all interrupt flags
		DMA2->IFCR = ( DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6 );
		
		// Clear any UART pending DMA requests
		LPUART1->CR3 &= ~USART_CR3_DMAT;
		
		// Enable DMA mode for transmitter
		LPUART1->CR3 |= USART_CR3_DMAT;
		
		// Enable DMA 2 stream 6
		DMA2_Channel6->CCR |= DMA_CCR_EN;
	}
}

void DS18B20_CMDReceive(const uint8_t * cmd, uint8_t size)
{
	if (cmd != NULL)
	{
		/*
		 *  Wait until DMA2 channel 7 is disabled
		 *  The enable flag shall reset when DMA transfer complete
		 */
		while((DMA_CCR_EN & DMA2_Channel7->CCR) == DMA_CCR_EN);
		
		// Memory buffer address
		DMA2_Channel7->CMAR = (uint32_t)cmd;
		
		// Number of data to be transfered
		DMA2_Channel7->CNDTR = (uint32_t)size;
		
		DMA2->IFCR = ( DMA_IFCR_CGIF7 | DMA_IFCR_CTCIF7 | DMA_IFCR_CHTIF7 | DMA_IFCR_CTEIF7 );
		
		// Clear any UART pending DMA requests
		LPUART1->CR3 &= ~USART_CR3_DMAR;
		
		// Enable DMA mode for Reception
		LPUART1->CR3 |= USART_CR3_DMAR;
		
		// Enable DMA 2 stream 7
		DMA2_Channel7->CCR |= DMA_CCR_EN;
	}
}

uint8_t DS18B20_CMDReset(void)
{
	uint16_t rx_byte = 0;
	uint8_t sensor = 0;
	
	// Disable LPUART1
	LPUART1->CR1 &= ~USART_CR1_UE;
	
	/**
	 * Baud Rate = 9600
	 * with Fck=4MHz, USARTDIV = 256*4000000/9600 = 106666.6667
	 * BRR = 106666 -> Baud Rate = 9599.97 -> 0.0003% error
	 */
	// Set Baudrate to 9600 baud @4MHz
	LPUART1->BRR = 106667U;
	
	// Enable LPUART1
	LPUART1->CR1 |= USART_CR1_UE;
	
	while ( (LPUART1->ISR & USART_ISR_TXE) != USART_ISR_TXE);
	
	LPUART1->TDR = RESET_PULSE;
	
	while ( (LPUART1->ISR & USART_ISR_TC) != USART_ISR_TC);
	
	rx_byte = LPUART1->RDR;
	
	if(( rx_byte != RESET_PULSE ) && (rx_byte != BIT_0)) // BIT_0 = PRESENCE
	{
		// Sensors detected
		sensor = 1U;
	}
	else
	{
		// Sensors not detected
		sensor = 0U;
	}
	
	// Disable LPUART1
	LPUART1->CR1 &= ~USART_CR1_UE;
	
	/**
	 * Baud Rate = 115200
	 * with Fck=4MHz, USARTDIV = 256*4000000/115200 = 8888.888
	 * BRR = 8889 -> Baud Rate = 115198.56 -> 0.001% error
	 */
	// Set Baudrate to 115200 baud @4MHz
	LPUART1->BRR = 8889U;
	
	// Enable LPUART1
	LPUART1->CR1 |= USART_CR1_UE;
	
	return sensor;
}

void DS18B20_GPIO_Init(void)
{
	// Enable GPIOC clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	
	// Configure PA0 and PA1 as Alternate function
	GPIOC->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1);
	GPIOC->MODER |=  (GPIO_MODER_MODE0_1 | GPIO_MODER_MODE1_1);
	
	// Set PA0 and PA1 to AF8 (LPUART1)
	GPIOC->AFR[0] &= ~(GPIO_AFRL_AFSEL0 | GPIO_AFRL_AFSEL1);
	GPIOC->AFR[0] |=  (GPIO_AFRL_AFSEL0_3 | GPIO_AFRL_AFSEL1_3);
	
	// Set output type PA0 TX as Open drain
	GPIOC->OTYPER |= (GPIO_OTYPER_OT0 |GPIO_OTYPER_OT1);
	
	// Set output to high speed*/
	GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0 | GPIO_OSPEEDR_OSPEED1);
	GPIOC->OSPEEDR |=  (GPIO_OSPEEDER_OSPEEDR0_1 | GPIO_OSPEEDER_OSPEEDR1_1);
	
	// Disable Pull resistors
	GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1);
}

/*
 * Configure DMA
 * LPUART1_TX is on DMA2 Channel 6 Request #4
 */
void DS18B20_TX_DMA_Init(void)
{
	// Enable DMA2 clock
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
	
	if((DMA_CCR_EN & DMA2_Channel6->CCR) == DMA_CCR_EN)
	{
		// DMA 2 channel 6 is enabled, shall be disabled first
		DMA2_Channel6->CCR &= ~DMA_CCR_EN;
		
		// Wait until EN bit is clear
		while((DMA_CCR_EN & DMA2_Channel6->CCR) == DMA_CCR_EN);
	}
	
	// DMA2 channel mapping (Channel 6 on Request 4)
	DMA2_CSELR->CSELR &= ~DMA_CSELR_C6S;
	DMA2_CSELR->CSELR |=  4U << DMA_CSELR_C6S_Pos;
	
	// Set priority level to high
	DMA2_Channel6->CCR |= DMA_CCR_PL;
	
	// Memory -> Peripheral
	DMA2_Channel6->CCR &= ~DMA_CCR_DIR;
	DMA2_Channel6->CCR |= DMA_CCR_DIR;
	
	// Set memory data size to 8-bits
	DMA2_Channel6->CCR &= ~DMA_CCR_MSIZE;
	
	// Set peripheral data size to 8-bits
	DMA2_Channel6->CCR &= ~DMA_CCR_PSIZE;
	
	// Disable peripheral increment
	DMA2_Channel6->CCR &= ~DMA_CCR_PINC;
	
	// Enable circular mode
	//DMA2_Channel6->CCR |= DMA_CCR_CIRC;
	
	// Enable memory increment
	DMA2_Channel6->CCR |= DMA_CCR_MINC;
	
	// Enable DMA transfer complete interrupt
	DMA2_Channel6->CCR |= DMA_CCR_TCIE;
	
	// Peripheral address
	DMA2_Channel6->CPAR = (uint32_t) &(LPUART1->TDR);
}

/*
 * Configure DMA
 * LPUART1_RX is on DMA2 Channel 7 Request #4
 */
void DS18B20_RX_DMA_Init(void)
{
	// Enable DMA2 clock
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
	
	if((DMA_CCR_EN & DMA2_Channel7->CCR) == DMA_CCR_EN)
	{
		// DMA 2 channel 7 is enabled, shall be disabled first
		DMA2_Channel7->CCR &= ~DMA_CCR_EN;

		// Wait until EN bit is clear
		while((DMA_CCR_EN & DMA2_Channel7->CCR) == DMA_CCR_EN);
	}
	
	// DMA2 channel mapping (Channel 7 on Request 4)
	DMA2_CSELR->CSELR &= ~DMA_CSELR_C7S;
	DMA2_CSELR->CSELR |=  4U << DMA_CSELR_C7S_Pos;
	
	// Set priority level to high
	DMA2_Channel7->CCR |= DMA_CCR_PL;
	
	// Peripheral -> Memory
	DMA2_Channel7->CCR &= ~DMA_CCR_DIR;
	
	// Set memory data size to 8-bits
	DMA2_Channel7->CCR &= ~DMA_CCR_MSIZE;
	
	// Set peripheral data size to 8-bits
	DMA2_Channel7->CCR &= ~DMA_CCR_PSIZE;
	
	// Disable peripheral increment
	DMA2_Channel7->CCR &= ~DMA_CCR_PINC;
	
	// Enable circular mode
	// DMA2_Channel7->CCR |= DMA_CCR_CIRC;
	
	// Enable memory increment
	DMA2_Channel7->CCR |= DMA_CCR_MINC;
	
	// Enable DMA transfer complete interrupt
	DMA2_Channel7->CCR |= DMA_CCR_TCIE;
	
	// Peripheral address
	DMA2_Channel7->CPAR = (uint32_t) &(LPUART1->RDR);
}

void DS18B20_LPUART1_Init(void)
{
	//Enable LPUART1 clock
	RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
	
	/**
	 * Clear LPUART1 configuration (reset state)
	 * 8-bit, 1 start, 1 stop, CTS/RTS disabled
	 */
	LPUART1->CR1 = 0x00000000U;
	LPUART1->CR2 = 0x00000000U;
	LPUART1->CR3 = 0x00000000U;
	
	//Select Single-wire Half-duplex mode
	LPUART1->CR3 |= USART_CR3_HDSEL;
}

void DS18B20_LPUART1_Enable(void)
{
	//Enable LPUART1
	LPUART1->CR1 |= USART_CR1_UE;
	
	//Enable transmitter
	LPUART1->CR1 |= USART_CR1_TE;
	
	//Enable receiver
	LPUART1->CR1 |= USART_CR1_RE;
}

void DS18B20_Process(void)
{
	uint8_t isSensor = 0U;
	// Send reset pulse
	isSensor = DS18B20_CMDReset();
	if(isSensor == 1)
	{
		// 12-bit resolution
		// Send temperature conversion command
		DS18B20_CMDTransmit(temp_convert, sizeof(temp_convert));
		
		// Wait conversion time
		delay(750);
		
		// Send reset pulse
		DS18B20_CMDReset();
		
		// Enable temperature data reception with DMA
		DS18B20_CMDReceive(temperatureData, sizeof(temperatureData));
		
		// Send temperature read command
		DS18B20_CMDTransmit(temp_read, sizeof(temp_read));
		
		// Wait until DMA receive temperature data
		while (temperatureDataReceived == 0);
		
		// Reset temperature data received flag
		temperatureDataReceived = 0;
		
		// Temporarily variable for extracting temperature data
		uint16_t temperature = 0;
		
		// Extract new temperature data
		for (uint8_t i = 16U; i < 32U; i++)
		{
			if (temperatureData[i] == BIT_1)
			{
				temperature = (temperature >> 1) | 0x8000U;
			}
			else
			{
				temperature = temperature >> 1;
			}
		}
		currentTemperature = temperature / 16.0;
	}
	else {
		currentTemperature = 0;
	}
}

void DMA2_Channel6_IRQHandler(void)
{
	// Test if this is a TC interrupt
	if ( (DMA2->ISR & DMA_ISR_TCIF6) == DMA_ISR_TCIF6 )
	{
		// Clear all interrupt flag
		DMA2->IFCR |= ( DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6 );
		
		// Enable DMA 2 stream 7
		DMA2_Channel6->CCR &= ~DMA_CCR_EN;
	}
}

void DMA2_Channel7_IRQHandler(void)
{
	// Test if this is a TC interrupt
	if ( (DMA2->ISR & DMA_ISR_TCIF7) == DMA_ISR_TCIF7 )
	{
		// Clear all interrupt flag
		DMA2->IFCR |= ( DMA_IFCR_CGIF7 | DMA_IFCR_CTCIF7 | DMA_IFCR_CHTIF7 | DMA_IFCR_CTEIF7 );
		
		// Enable DMA 2 stream 6
		DMA2_Channel7->CCR &= ~DMA_CCR_EN;
		
		// Set transfer complete flag
		temperatureDataReceived = 1;
	}
}
