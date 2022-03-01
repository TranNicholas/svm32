#include "SysClock.h"

void System_Clock_Init(void)
{
	// Start LSE (for MSI PLL hardware calibration) 
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN; // Enable writing of Battery/Backup domain 
	PWR->CR1 |= PWR_CR1_DBP;
	
	RCC->BDCR &= ~RCC_BDCR_LSEDRV; // Set LSE driving to medium effort 
	RCC->BDCR |= RCC_BDCR_LSEDRV_1;
	
	RCC->BDCR |= RCC_BDCR_LSEON; // Start LSE 
	
	// Wait until LSE is ready 
	while ((RCC->BDCR & RCC_BDCR_LSERDY) != RCC_BDCR_LSERDY);
	
	// Enable MSI PLL auto-calibration 
	RCC->CR |= RCC_CR_MSIPLLEN;
	
	// Reset MSI range 
	RCC->CR &= ~RCC_CR_MSIRANGE;
	
	// Set MSI range 
	RCC->CR |= RCC_CR_MSIRANGE_6; // For 4MHz 
	RCC->CR |= RCC_CR_MSIRGSEL; // MSI range is provided by CR register 
	
	// Start MSI 
	RCC->CR |= RCC_CR_MSION;
	
	// Wait until MSI is ready 
	while ((RCC->CR & RCC_CR_MSIRDY) != RCC_CR_MSIRDY);
	
	// Set voltage range 1 as MCU will run at 4MHz 
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	PWR->CR1 |= PWR_CR1_VOS_0;
	PWR->CR1 &= ~PWR_CR1_VOS_1;

	// Wait until VOSF bit is cleared (regulator ready) 
	while ((PWR->SR2 & PWR_SR2_VOSF) == PWR_SR2_VOSF);

	// Configure FLASH with prefetch and 0 WS 
	FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_0WS;

	// Configure AHB/APB prescalers
	// AHB  Prescaler = /1	-> 4MHz
	// APB1 Prescaler = /1  -> 4MHz
	// APB2 Prescaler = /1  -> 4MHz
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;

	// Wait until MSI is ready 
	while((RCC->CR & RCC_CR_MSIRDY) != RCC_CR_MSIRDY);

	// Select the main MSI as system clock source 
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_MSI;

	// Wait until MSI is switched on 
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_MSI);

	// Set MSI as the CLK48 (actually 4MHz) clock source for SDMMC 
	RCC->CCIPR |= RCC_CCIPR_CLK48SEL;

	// Start RTC clock 
	RCC->BDCR |= RCC_BDCR_LSCOSEL; // Select LSE as Low Speed Clock for RTC 
	RCC->BDCR |= RCC_BDCR_RTCSEL_0; // Select LSE as RTC clock 
	RCC->BDCR |= RCC_BDCR_RTCEN; // Enable RTC clock 
}
