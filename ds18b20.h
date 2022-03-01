#ifndef __STM32L476R_NUCLEO_DS18B20_H
#define __STM32L476R_NUCLEO_DS18B20_H

#include "stm32l4xx.h"

void DS18B20_GPIO_Init (void);

void DS18B20_TX_DMA_Init (void);
void DS18B20_RX_DMA_Init (void);

void DS18B20_LPUART1_Init (void);
void DS18B20_LPUART1_Enable (void);

uint8_t DS18B20_CMDReset (void);
void DS18B20_CMDTransmit (const uint8_t * cmd, uint8_t size);
void DS18B20_CMDReceive (const uint8_t * cmd, uint8_t size);

void DS18B20_Process (void);

#endif /* __STM32L476R_NUCLEO_DS18B20_H */
