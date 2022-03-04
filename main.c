#include "stm32l476xx.h"
#include "SysClock.h"
#include "SysTimer.h"
#include "UART.h"
#include "ds18b20.h"
#include "RTC.h"
#include "alarm.h"
#include <stdio.h>
#include <stdbool.h>

extern volatile double currentTemperature;
extern volatile uint16_t minutes;

static int tempHour, tempMinute, numOfArgs;
static double tempTemp;

static uint16_t cookingTime; // in minutes
static double cookingTemperature; // in Celsius

static const char* STATUS2STR[] = {"REST", "WARMING", "COOKING", "PAUSED", "FINISHED"};
static enum STATES {REST, WARMING, COOKING, PAUSED, FINISHED} status = REST;
static enum COMMANDS {INVALID, REPORT, START, PAUSE, STOP, TIME, TEMP} command = INVALID;
static char buffer[1024] = {0};
bool startCooking = true;
char strTime[12] = {0};
char strDate[12] = {0};

int main(void)
{
	// Configure System Clock for 4MHz (with LSE calibration)
	System_Clock_Init();
	
	// RESTize SysTick (no start)
	SysTick_Init();
	
	// RESTize Console
	Init_USARTx(1);
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_EnableIRQ(USART1_IRQn);
	
	// RESTize RTC
	RTC_Init();
	RTC_Alarm_Enable();
	RTC_Set_Alarm();
	
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
		switch(status) {
			case COOKING: // cook the food for cookingTime
				if (minutes < cookingTime) {
					DS18B20_Process();
				} else {
					status = FINISHED;
				}
				break;
			case REST: // REST state before start cooking/warming
				if (command == START) {
					// start warming the water/check the water is at correct temp
					status = WARMING;
				}
				break;
			case WARMING: // warm up water to cookingTemperature
				status = COOKING; // start cooking, water reached desired temp
				Alarm_Enable();
				break;
			case PAUSED: // pause cooking process temporarily
				break;
			case FINISHED: // finished cooking, ping user and maintain temperature or shut off
				break;
		}
	}
}


enum COMMANDS getPrefix(void) {
	if (strncmp(buffer, "REPORT", 5) == 0) {
		return REPORT;
	}
	if (strncmp(buffer, "TEMP=", 5) == 0) {
		return TEMP;
	}
	if (strncmp(buffer, "TIME=", 5) == 0) {
		return TIME;
	}
	if (strncmp(buffer, "START", 5) == 0) {
		return START;
	}
	if (strncmp(buffer, "STOP", 4) == 0) {
		return STOP;
	}
	if (strncmp(buffer, "PAUSE", 5) == 0) {
		return PAUSE;
	}
	return INVALID;
}

void USART1_IRQHandler(void) {
	if (USART1->ISR & USART_ISR_RXNE) {
		fgets(buffer, sizeof(buffer), stdin);
		command = getPrefix();
		switch (command) {
			case TEMP:
				sscanf(buffer + 5, "%lf", &tempTemp);
				printf("SETTING TEMPERATURE TO %f F\n", tempTemp);
				tempTemp = (tempTemp - 32) * 5 / 9; // convert to Celsius
				if (tempTemp > 20 && tempTemp < 95) {
					cookingTemperature = tempTemp;
				}
				break;
			case TIME:
				numOfArgs = sscanf(buffer + 5, "%u%u", &tempHour, &tempMinute);
				switch(numOfArgs) {
					case 2:
						tempMinute += tempHour * 60;
						break;
					case 1:
						tempMinute = tempHour;
						break;
					default:
						tempMinute = 0;
						break;
				}
				if (tempMinute > 0 && tempMinute < 2880) {// 48 hours
					cookingTime = tempMinute;
				}
				break;
			case REPORT:
				printf("SET TO %f C for %d MINUTES\n", cookingTemperature, cookingTime);
				printf("CURRENT STATE: %s\n", STATUS2STR[status]);
				if (status == COOKING || status == PAUSED) {
					printf("CURRENT TEMPERATURE: %f, MINUTES ELAPSED: %d\n", currentTemperature, minutes);
				}
				break;
			case INVALID:
				printf("INVALID COMMAND\n");
				break;
			default: // START/STOP/PAUSE
				break;
		}
	}
}

