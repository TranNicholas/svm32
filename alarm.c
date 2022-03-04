#include "RTC.h"
#include "alarm.h"

volatile uint16_t minutes = 0;

void RTC_Set_Alarm(void) {
	//disable both alarms
	RTC->CR &= ~RTC_CR_ALRAE;
	
	// disable RTC write protection
	RTC_Disable_Write_Protection();
	
	//clear alarm enable and interrupt enable for both alarms
	RTC->CR &= ~RTC_CR_ALRAE;
	RTC->CR &= ~RTC_CR_ALRAIE;
	// wait until alarm A is accessible
	while ((RTC->ISR & RTC_ISR_ALRAWF) != RTC_ISR_ALRAWF);
	
	// set off alarm A when second is 0 i.e. once a minute
	RTC->ALRMAR |= RTC_ALRMAR_MSK4;
	RTC->ALRMAR |= RTC_ALRMAR_MSK3;
	RTC->ALRMAR |= RTC_ALRMAR_MSK2;
	RTC->ALRMAR &= ~RTC_ALRMAR_MSK1;
	
	RTC->ALRMAR &= ~RTC_ALRMAR_ST;
	RTC->ALRMAR &= ~RTC_ALRMAR_SU;
	RTC->ALRMAR |= (RTC_ALRMAR_ST & 0x00U);
	RTC->ALRMAR |= (RTC_ALRMAR_SU & 0x00U);
	
	// enable interrupts
	RTC->CR |= RTC_CR_ALRAIE;
	
	// reenable write protection for RTC
	RTC_Enable_Write_Protection();
}

void RTC_Alarm_Enable(void) {
	// configure interrupt to trigger on rising edge
	EXTI->RTSR1 |= EXTI_RTSR1_RT18;
	
	//Set the interrupt mask and event mask
	EXTI->IMR1 |= EXTI_IMR1_IM18;
	EXTI->EMR1 |= EXTI_EMR1_EM18;
	
	// Clear pending interrupt
	EXTI->PR1 |= EXTI_PR1_PIF18;
	
	// Enable the interrupt in NVIC and set to highest priority.
	NVIC_EnableIRQ(RTC_Alarm_IRQn);
	NVIC_SetPriority(RTC_Alarm_IRQn, 0);
}

void RTC_Alarm_IRQHandler(void) {
	// when alarm A is triggered, toggle LED
	if (RTC->ISR & RTC_ISR_ALRAF) {
		RTC->ISR &= ~RTC_ISR_ALRAF;
		minutes++;
	}
	// clear alarm event flag and interrupt pending bit
	EXTI->PR1 |= EXTI_PR1_PIF18;
}

void Alarm_Enable(void) {
	RTC_Disable_Write_Protection();
	RTC->CR |= RTC_CR_ALRAE;
	while ((RTC->ISR & RTC_ISR_ALRAWF) != RTC_ISR_ALRAWF);
	RTC_Enable_Write_Protection();
}

void Alarm_Disable(void) {
	RTC_Disable_Write_Protection();
	RTC->CR &= ~RTC_CR_ALRAE;
	while ((RTC->ISR & RTC_ISR_ALRAWF) != RTC_ISR_ALRAWF);
	RTC_Enable_Write_Protection();
}
