#include "I2C.h"
#include "SysTimer.h"

// heaviled based off Grenoble-INP France driver for LCD HD44780 driven through PCF8574 expander
// https://community.st.com/s/question/0D50X00009sUBHFSA4/enpresentationstm32i2clcdhd44780v2

// PCF8574AT
#define SLAVE_ADDRESS_LCD 0x4E
#define LCD_I2C I2C1

extern void Error_Handler(void);

// Inter-integrated Circuit Interface (I2C)
// up to 100 Kbit/s in the standard mode, 
// up to 400 Kbit/s in the fast mode, and 
// up to 3.4 Mbit/s in the high-speed mode.

// Recommended external pull-up resistance is 
// 4.7 kOmh for low speed, 
// 3.0 kOmh for the standard mode, and 
// 1.0 kOmh for the fast mode
	
//===============================================================================
//                        I2C GPIO Initialization
//===============================================================================
void I2C_GPIO_Init(void) {
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	// alternate function mode
	GPIOB->MODER &= ~GPIO_MODER_MODE8_0 & ~GPIO_MODER_MODE9_0;
	GPIOB->MODER |= GPIO_MODER_MODE8_1 | GPIO_MODER_MODE9_1;
	// open-drain output type
	GPIOB->OTYPER |= GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9;
	// very high speed
	GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8 | GPIO_OSPEEDER_OSPEEDR9;
	// pull-up resistors
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR8_0 | GPIO_PUPDR_PUPDR9_0;
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR8_1 & ~GPIO_PUPDR_PUPDR9_1;
	// selct AF 4 for PB8 and PB9
	GPIOB->AFR[1] |= GPIO_AFRH_AFSEL8_2 | GPIO_AFRH_AFSEL9_2;
	GPIOB->AFR[1] &= ~GPIO_AFRH_AFSEL8_0 & ~GPIO_AFRH_AFSEL8_1 &
					 ~GPIO_AFRH_AFSEL8_3 & ~GPIO_AFRH_AFSEL9_0 & 
					 ~GPIO_AFRH_AFSEL9_1 & ~GPIO_AFRH_AFSEL9_3;
}
	
#define I2C_TIMINGR_PRESC_POS	28
#define I2C_TIMINGR_SCLDEL_POS	20
#define I2C_TIMINGR_SDADEL_POS	16
#define I2C_TIMINGR_SCLH_POS	8
#define I2C_TIMINGR_SCLL_POS	0

//===============================================================================
//                          I2C Initialization
//===============================================================================
void I2C_Initialization(void){
	uint32_t OwnAddr = 0x52U;
	
	RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
	RCC->CCIPR |= RCC_CCIPR_I2C1SEL_0;
	RCC->CCIPR &= ~RCC_CCIPR_I2C1SEL_1;
	RCC->APB1RSTR1 |= RCC_APB1RSTR1_I2C1RST;
	RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C1RST;
	
	I2C1->CR1 &= ~I2C_CR1_PE; // disable I2C
	while ((I2C1->CR1 & I2C_CR1_PE) == I2C_CR1_PE); // wait 3 APB clock cycles
	I2C1->CR1 &= ~I2C_CR1_ANFOFF;
	I2C1->CR1 &= ~I2C_CR1_DNF;
	I2C1->CR1 |= I2C_CR1_ERRIE;
	I2C1->CR1 &= ~I2C_CR1_NOSTRETCH;
	I2C1->CR2 &= ~I2C_CR2_ADD10; // 7-bit addressing mode
	I2C1->CR2 |= I2C_CR2_AUTOEND | I2C_CR2_NACK;
	
	// 8 MHz CLK, 125 ns period
	// SCLDEL - 4.7us setup time
	// SDADEL - 4.0us hold time
	// SCLH - 4us high time
	// SCLL - 4.7us low time
	I2C1->TIMINGR |= (I2C_TIMINGR_PRESC & (3U << I2C_TIMINGR_PRESC_POS)) |
					 (I2C_TIMINGR_SCLDEL & (4U << I2C_TIMINGR_SCLDEL_POS)) |
					 (I2C_TIMINGR_SDADEL & (3U << I2C_TIMINGR_SDADEL_POS)) |
					 (I2C_TIMINGR_SCLH & (3U << I2C_TIMINGR_SCLH_POS)) |
					 (I2C_TIMINGR_SCLL & (4U << I2C_TIMINGR_SCLL_POS));
	
	I2C1->OAR1 &= ~I2C_OAR1_OA1EN;
	while ((I2C1->OAR1 & I2C_OAR1_OA1EN) == I2C_OAR1_OA1EN);
	I2C1->OAR2 &= ~I2C_OAR2_OA2EN;
	while ((I2C1->OAR2 & I2C_OAR2_OA2EN) == I2C_OAR2_OA2EN); // wait until not enabled
	I2C1->OAR1 &= ~I2C_OAR1_OA1MODE;
	I2C1->OAR1 |= (OwnAddr << 1) & I2C_OAR1_OA1;
	I2C1->OAR1 |= I2C_OAR1_OA1EN;
	
	I2C1->CR1 |= I2C_CR1_PE;
}

//===============================================================================
//                           I2C Start
// Master generates START condition:
//    -- Slave address: 7 bits
//    -- Automatically generate a STOP condition after all bytes have been transmitted 
// Direction = 0: Master requests a write transfer
// Direction = 1: Master requests a read transfer
//=============================================================================== 
int8_t I2C_Start(I2C_TypeDef * I2Cx, uint32_t DevAddress, uint8_t Size, uint8_t Direction) {
	
	// Direction = 0: Master requests a write transfer
	// Direction = 1: Master requests a read transfer
	
	uint32_t tmpreg = 0;
	
	// This bit is set by software, and cleared by hardware after the Start followed by the address
	// sequence is sent, by an arbitration loss, by a timeout error detection, or when PE = 0.
	tmpreg = I2Cx->CR2;
	
	tmpreg &= (uint32_t)~((uint32_t)(I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RELOAD | I2C_CR2_AUTOEND | I2C_CR2_RD_WRN | I2C_CR2_START | I2C_CR2_STOP));
	
	if (Direction == READ_FROM_SLAVE)
		tmpreg |= I2C_CR2_RD_WRN;  // Read from Slave
	else
		tmpreg &= ~I2C_CR2_RD_WRN; // Write to Slave
		
	tmpreg |= (uint32_t)(((uint32_t)DevAddress & I2C_CR2_SADD) | (((uint32_t)Size << 16 ) & I2C_CR2_NBYTES));
	
	tmpreg |= I2C_CR2_START;
	
	I2Cx->CR2 = tmpreg; 
	
   	return 0;  // Success
}

//===============================================================================
//                           I2C Stop
//=============================================================================== 
void I2C_Stop(I2C_TypeDef * I2Cx){
	// Master: Generate STOP bit after the current byte has been transferred 
	I2Cx->CR2 |= I2C_CR2_STOP;								
	// Wait until STOPF flag is reset
	while( (I2Cx->ISR & I2C_ISR_STOPF) == 0 ); 
}

//===============================================================================
//                           Wait for the bus is ready
//=============================================================================== 
void I2C_WaitLineIdle(I2C_TypeDef * I2Cx){
	// Wait until I2C bus is ready
	while( (I2Cx->ISR & I2C_ISR_BUSY) == I2C_ISR_BUSY );	// If busy, wait
}

//===============================================================================
//                           I2C Send Data
//=============================================================================== 
int8_t I2C_SendData(I2C_TypeDef * I2Cx, uint8_t DeviceAddress, uint8_t *pData, uint8_t Size) {
	int i;
	
	if (Size <= 0 || pData == NULL) return -1;
	
	I2C_WaitLineIdle(I2Cx);
	
	if (I2C_Start(I2Cx, DeviceAddress, Size, WRITE_TO_SLAVE) < 0 ) return -1;

	// Send Data
	// Write the first data in DR register
	// while((I2Cx->ISR & I2C_ISR_TXE) == 0);
	// I2Cx->TXDR = pData[0] & I2C_TXDR_TXDATA;  

	for (i = 0; i < Size; i++) {
		// TXE is set by hardware when the I2C_TXDR register is empty. It is cleared when the next
		// data to be sent is written in the I2C_TXDR register.
		// while( (I2Cx->ISR & I2C_ISR_TXE) == 0 ); 

		// TXIS bit is set by hardware when the I2C_TXDR register is empty and the data to be
		// transmitted must be written in the I2C_TXDR register. It is cleared when the next data to be
		// sent is written in the I2C_TXDR register.
		// The TXIS flag is not set when a NACK is received.
		while((I2Cx->ISR & I2C_ISR_TXIS) == 0 );
		I2Cx->TXDR = pData[i] & I2C_TXDR_TXDATA;  // TXE is cleared by writing to the TXDR register.
	}
	
	// Wait until TC flag is set 
	while((I2Cx->ISR & I2C_ISR_TC) == 0 && (I2Cx->ISR & I2C_ISR_NACKF) == 0);
	
	if( (I2Cx->ISR & I2C_ISR_NACKF) != 0 ) return -1;

	I2C_Stop(I2Cx);
	return 0;
}


//===============================================================================
//                           I2C Receive Data
//=============================================================================== 
int8_t I2C_ReceiveData(I2C_TypeDef * I2Cx, uint8_t DeviceAddress, uint8_t *pData, uint8_t Size) {
	int i;
	
	if(Size <= 0 || pData == NULL) return -1;

	I2C_WaitLineIdle(I2Cx);

	I2C_Start(I2Cx, DeviceAddress, Size, READ_FROM_SLAVE); // 0 = sending data to the slave, 1 = receiving data from the slave
						  	
	for (i = 0; i < Size; i++) {
		// Wait until RXNE flag is set 	
		while( (I2Cx->ISR & I2C_ISR_RXNE) == 0 );
		pData[i] = I2Cx->RXDR & I2C_RXDR_RXDATA;
	}
	
	// Wait until TCR flag is set 
	while((I2Cx->ISR & I2C_ISR_TC) == 0);
	
	I2C_Stop(I2Cx);
	
	return 0;
}

void LCD_Send_CMD(char cmd) {
	// 4 bit data represention, 4 LSB are dropped
	// to send a byte, send two 4-bit nibbles (where the 4-bits are in the left half)
	char data_u, data_l;
	uint8_t data_t[4];
	data_u = (cmd&0xf0);
	data_l = ((cmd<<4)&0xf0);
	data_t[0] = data_u|0x0C;  //en=1, rs=0
	data_t[1] = data_u|0x08;  //en=0, rs=0
	data_t[2] = data_l|0x0C;  //en=1, rs=0
	data_t[3] = data_l|0x08;  //en=0, rs=0
	I2C_SendData(LCD_I2C, SLAVE_ADDRESS_LCD, data_t, 4);
}

void LCD_Send_Data(char data) {
	char data_u, data_l;
	uint8_t data_t[4];
	data_u = (data&0xf0);
	data_l = ((data<<4)&0xf0);
	data_t[0] = data_u|0x0D;  //en=1, rs=1
	data_t[1] = data_u|0x09;  //en=0, rs=1
	data_t[2] = data_l|0x0D;  //en=1, rs=1
	data_t[3] = data_l|0x09;  //en=0, rs=1
	I2C_SendData(LCD_I2C, SLAVE_ADDRESS_LCD, data_t, 4);
}
void LCD_Init(void) {
	  //Initialization of HD44780-based LCD (4-bit HW)
	LCD_Send_CMD(0x33);
	LCD_Send_CMD(0x32);
	LCD_Send_CMD(0x28);   //Function Set 4-bit mode
	LCD_Send_CMD(0x0C);   //Display On/Off Control
	LCD_Send_CMD(0x06);   //Entry mode set
	LCD_Send_CMD(0x02);   //Clear Display
	//Minimum delay to wait before driving LCD module
	delay(200);
}

void LCD_BL(bool val) {
	if (val) {
		LCD_Send_CMD(0x0C); // set backlight on (turn on LCD)
	} else {
		LCD_Send_CMD(0x08); // set backlight off (turn off LCD)
	}
}

void LCD_Clear(void) { // clear display and move cursor to home (1, 1)
	LCD_Send_CMD(0x01);
	delay(2);
}

void LCD_Home(void) { // move cursor to home position (1, 1)
	LCD_Send_CMD(0x02);
	delay(2);
}

void LCD_Locate(uint8_t row, uint8_t column) {
	column--; // 1-index to 0-index
	switch(row) {
		case 1:
			LCD_Send_CMD(column | 0x80);
			break;
		case 2:
			LCD_Send_CMD(column | 0xC0);
			break;
		case 3:
			LCD_Send_CMD(column | 0x94);
			break;
		case 4:
			LCD_Send_CMD(column | 0xD4);
			break;
		default:
			break;
	}
}

void LCD_printchar(char c) {
	LCD_Send_Data(c);
}

void LCD_print_str(char* str) {
	while (*str) LCD_Send_Data(*str++);
}
