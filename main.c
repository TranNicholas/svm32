#include "stm32l476xx.h"
#include "SysClock.h"
#include "SysTimer.h"
#include "UART.h"
#include "ds18b20.h"
#include "RTC.h"
#include "alarm.h"
#include "relay.h"
#include "I2C.h"
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

/* PIN LAYOUT
 * PC0/PC1 -> LPUART -> One-Wire DS18B20 Thermal Sensor
 * PB6/PB7 -> UART1 -> HM-10 Bluetooth LE UART UART
 * PB8/PB9 -> I2C1 -> LCD I2C 2004
 * PA13 -> IoT Relay/Pump
 */


extern volatile double currentTemperature;
extern volatile uint16_t minutes;

static int tempHour, tempMinute, numOfArgs;
static double tempTemp;

static uint16_t cookingTime; // in minutes
static double cookingTemperature; // in Celsius


static const char* RELAY2STR[] = {"Off", "On"};
static const char* STATUS2STR[] = {"Rest", "Warming", "Cooking", "Paused", "Finished"};
static enum STATES {REST, WARMING, COOKING, PAUSED, FINISHED} status = REST;
static enum COMMANDS {INVALID, REPORT, START, PAUSE, STOP, TIME, TEMP} command = INVALID;
static char buffer[1024] = {0};
static char lcd_buf[21] = {0};

static const uint32_t windowSize = 5000U;
static uint32_t lastTime, currentTime, windowStart, output;
static double errSum, lastErr;
static double kp = 2, ki = 5, kd = 1;

void compute(void) {
	uint32_t now = SysTick->VAL;
	
	double timeChange = (double) (now - lastTime);
	
	double error = cookingTemperature - currentTemperature;
	errSum += (error * timeChange);
	double dErr = (error - lastErr) / timeChange;
	
	output = kp * error + ki * errSum + kd*dErr;
	lastErr = error;
	lastTime = now;
}

int main(void)
{
	// Configure System Clock for 4MHz (with LSE calibration)
	System_Clock_Init();
	
	// Initialize SysTick (no start)
	SysTick_Init();
	
	// Initialize Console
	UART1_Init();
	UART1_GPIO_Init();
	USART_Init(USART1);
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_EnableIRQ(USART1_IRQn);
	
	
	// Initialize RTC
	RTC_Init();
	RTC_Alarm_Enable();
	RTC_Set_Alarm();
	
	// Initialize Relay
	Relay_Init();
	
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
	
	// Initialize Screen
	I2C_GPIO_Init();
	I2C_Initialization();
	LCD_Init();
	LCD_Clear();
	
	// Infinite loop
	while(1)
	{
		DS18B20_Process();
		LCD_Locate(1, 1);
		snprintf(lcd_buf, 21, "Status: %s                    ", STATUS2STR[status]);
		LCD_print_str(lcd_buf);
		
		LCD_Locate(2, 1);
		snprintf(lcd_buf, 21, "Temp: %.2f F                    ", currentTemperature * 9/5 + 32);
		LCD_print_str(lcd_buf);
		
		LCD_Locate(3, 1);
		snprintf(lcd_buf, 21, "Timer: %d Minutes                    ", cookingTime - minutes);
		LCD_print_str(lcd_buf);
		
		LCD_Locate(4, 1);
		snprintf(lcd_buf, 21, "Power: %s                    ", RELAY2STR[(GPIOA->ODR & GPIO_ODR_OD13) == GPIO_ODR_OD13]);
		LCD_print_str(lcd_buf);
		switch(status) {
			case COOKING: // cook the food for cookingTime
				if (command == PAUSE) {
					command = INVALID; // clear command
					status = PAUSED;
					Alarm_Disable();
					Relay_Off();
				} else if (command == STOP) {
					command = INVALID; // clear command
					status = REST;
					Alarm_Disable();
					Relay_Off();
					minutes = 0;
				}else if (minutes < cookingTime) {
					currentTime = SysTick->VAL;
					compute();
					if (output > currentTime - windowStart) { // time proportioning control
						Relay_On();
					} else {
						Relay_Off();
					}
				} else {
					Relay_Off();
					Alarm_Disable();
					status = FINISHED;
				}
				break;
			case REST: // REST state before start cooking/warming
				if (command == START) {
					command = INVALID; // clear command
					// start warming the water/check the water is at correct temp
					status = WARMING;
					Relay_On();
				}
				break;
			case WARMING: // warm up water to cookingTemperature
				if (command == PAUSE || command == STOP) {
					command = INVALID; // clear command
					status = REST; // off
					Alarm_Disable();
					Relay_Off();
				} else if (currentTemperature >= cookingTemperature) {
					status = COOKING; // start cooking, water reached desired temp
					Alarm_Enable();
				}
				break;
			case PAUSED: // pause cooking process temporarily
				if (command == START) {
					command = INVALID; // clear command
					status = WARMING;
					Relay_On();
				} else if (command == STOP) {
					command = INVALID; // clear command
					status = REST;
					minutes = 0;
				}
				break;
			case FINISHED: // finished cooking, ping user and maintain temperature or shut off
				if (command == START) {
					command = INVALID; // clear command
					status = WARMING;
					Relay_On();
				} else if (command == STOP) {
					command = INVALID; // clear command
					status = REST;
					Relay_Off(); // should already be off but just in case
				}
				break;
		}
	}
}

enum COMMANDS getPrefix(void) {
	if (strncmp(buffer, "REPORT", 5) == 0) {
		return REPORT;
	}
	if (strncmp(buffer, "TEMP", 4) == 0) {
		return TEMP;
	}
	if (strncmp(buffer, "TIME", 4) == 0) {
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
		USART1->ISR &= ~USART_ISR_RXNE;
		fgets(buffer, sizeof(buffer), stdin);
		for (int i = 0; buffer[i] != '\0'; i++) {
			buffer[i] = toupper(buffer[i]);
		}
		command = getPrefix();
		switch (command) {
			case TEMP:
				sscanf(buffer + 4, "%lf", &tempTemp);
				tempTemp = (tempTemp - 32) * 5 / 9; // convert to Celsius
				if (tempTemp > 20 && tempTemp < 95) {
					printf("SETTING TEMPERATURE TO %f F\n", tempTemp * 9/5 + 32);
					cookingTemperature = tempTemp;
				} else {
					printf("INVALID TEMPERATURE\n");
				}
				break;
			case TIME:
				numOfArgs = sscanf(buffer + 4, "%u%u", &tempHour, &tempMinute);
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
					printf("SETTING COOK TIME TO %d MINUTES\n", tempMinute);
					cookingTime = tempMinute;
				} else {
					printf("INVALID COOK TIME\n");
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
			default:
				printf("COMMAND ACKNOWLEDGED\n");
				break;
		}
	}
}
