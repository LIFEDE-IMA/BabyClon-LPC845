/*
 * i2c.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "i2c.h"

I2C_Type* I2Cs[] = {I2C0, I2C1, I2C2, I2C3};

I2C *i2cInstance[TOTAL_I2C_SOURCES] = {nullptr, nullptr, nullptr, nullptr};

I2C::I2C(uint8_t i2c) : m_i2c(I2Cs[i2c]){
	i2cInstance[i2c] = this;
	m_i2cState = i2c_states::IDLE;
	m_data = m_mstState = m_slvAddr = m_slvRegAddr = 0;
	m_dataRdy = false;


	I2C::init(i2c);
}

void I2C::init(uint8_t i2c){	//	NVIC (Cap. 7), SYSCON (Cap. 8), SWITCH MATRIX (Cap. 10), IOCON (Cap. 11), I2C (Cap. 19)
	// Habilitacion de clock
	SYSCON->FCLKSEL[(5 + i2c)] = 0x1;	//	Clk source for I2Cx = main (30MHz)
	SYSCON->SYSAHBCLKCTRL0 |= ((1 << 6) | (1 << 7) | (1 << 18));	//	Enables GPIO0, SWM and IOCON clks

	switch(i2c){
		case 0:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 5);	//	Enables I2C0 clk
			// Enable I2C pins (PIO0_10 SCL, PIO0_11 SDA)
			SWM->PINENABLE0 &= ~((1 << 12) | (1 << 13));	//	Enables SDA Y SCL
			// Reset I2C0 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 5);	//	RESET I2C0
			SYSCON->PRESETCTRL0 |= (1 << 5);	//	RELEASE RESET I2C0
			//	Config pins with IOCON register
			IOCON->PIO[8] &= ~((1 << 9) | (1 << 8)); // SCL: I2C
			IOCON->PIO[7] &= ~((1 << 9) | (1 << 8)); // SDA: I2C
			break;

		case 1:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 21);	//	Enables I2C1 clk
			// Enable I2C pins
			// Reset I2C1 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 21);	//	RESET I2C1
			SYSCON->PRESETCTRL0 |= (1 << 21);	//	RELEASE RESET I2C1
			//	Config pins with IOCON register
			break;

		case 2:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 22);	//	Enables I2C2 clk
			// Enable I2C pins
			// Reset I2C2 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 22);	//	RESET I2C2
			SYSCON->PRESETCTRL0 |= (1 << 22);	//	RELEASE RESET I2C2
			//	Config pins with IOCON register
			break;

		case 3:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 23);	//	Enables I2C3 clk
			// Enable I2C pins
			// Reset I2C3 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 23);	//	RESET I2C3
			SYSCON->PRESETCTRL0 |= (1 << 23);	//	RELEASE RESET I2C3
			//	Config pins with IOCON register
			break;

		default:
			//	ERROR
			break;
	}

	// Clk config (100 kHz)
	m_i2c->CFG = (0 << 0);	//	Disables I2C
	m_i2c->CLKDIV = 74;	//	clkBase_I2C = PeripheralCLK / (CLKDIV + 1) = 30Mhz / 75 = 400kHz
	m_i2c->MSTTIME &= ~(0x7F);	//	Clears register
	m_i2c->MSTTIME |= ((0 << 0) | (0 << 4));	//	2 low cycles & 2 high cycles -> freq_i2c = clkBase / (c_high + c_low) = 400kHz/(2 + 2) = 100kHz
	m_i2c->CFG |= (1 << 0);	//	Enables I2C (master mode)

	I2C::enableNVIC_int(i2c);
}

void I2C::enableNVIC_int(uint8_t i2c){
	switch(i2c){
		case 0:
			NVIC->ISER[0] = (1 << I2C0_IRQn);	//	Enables NVIC interrupt
			break;

		case 1:
			NVIC->ISER[0] = (1 << I2C1_IRQn);	//	Enables NVIC interrupt
			break;

		case 2:
			NVIC->ISER[0] = (1 << I2C2_IRQn);	//	Enables NVIC interrupt
			break;

		case 3:
			NVIC->ISER[0] = (1 << I2C3_IRQn);	//	Enables NVIC interrupt
			break;

		default:
			//	ERROR
			break;
	}
}

void I2C::enableMasterInt(){ m_i2c->INTENSET |= (1 << 0); }	//	Enable Master Pending Interrupt

void I2C::disableMasterInt(){ m_i2c->INTENCLR = (1 << 0); }	//	Disable Master Pending Interrupt


void I2C::startReading(uint8_t slvAddress, uint8_t slvRegAddress){
	if(m_i2cState != i2c_states::IDLE){	//	Bus is busy
		return;
	}

	m_slvAddr = slvAddress;
	m_slvRegAddr = slvRegAddress;

	m_dataRdy = false;

	m_i2c->MSTDAT = ((m_slvAddr << 1) | 0); 	// Slave address + write
	m_i2c->MSTCTL = (1 << 1);            		// START
	m_i2cState = i2c_states::SEND_ADDR_W;

	I2C::enableMasterInt();	//	Enable Master Pending Interrupt

	//while((I2C0->STAT & 1)==0);	//	Waits MSTPENDING = 1
}

void I2C::isrHandler(){
	if(!(m_i2c->STAT & 1)){	//	MSTPENDING != 1
		return;
	}

	m_mstState = ((m_i2c->STAT >> 1) & 0x7);

	if((m_mstState == NACK_ADDR) || (m_mstState == NACK_DATA)){	//	NACK
		m_i2c->MSTCTL = (1 << 2);	//	STOP
		I2C::disableMasterInt();	//	Disable Master Pending Interrupt
		m_i2cState = i2c_states::IDLE;
		return;
	}

	switch(m_i2cState){
		case i2c_states::SEND_ADDR_W:
			if(m_mstState == TRANSMIT_RDY){		//	Transmit Rdy
				m_i2c->MSTDAT = m_slvRegAddr;	//	Sends which register wants to read
				m_i2c->MSTCTL = (1 << 0);		//	CONTINUE
				m_i2cState = i2c_states::SEND_REG;
				//while((I2C0->STAT & 1)==0);	//	Waits MSTPENDING = 1
			}
			break;

		case i2c_states::SEND_REG:
			if(m_mstState == TRANSMIT_RDY){				//	Transmit Rdy
				m_i2c->MSTDAT = ((m_slvAddr << 1) | 1);	//	Slave address + read
				m_i2c->MSTCTL = (1 << 1);				//	REPEATED START
				m_i2cState = i2c_states::REPEATED_START;
				//while((I2C0->STAT & 1)==0);	//	Waits MSTPENDING = 1
			}
			break;

		case i2c_states::REPEATED_START:
			if(m_mstState == RECEIVE_RDY){			//	Receive Rdy
				m_data = ((0xFF)&(m_i2c->MSTDAT));	//	low_data_byte
				m_i2c->MSTCTL = (1 << 0); 			//	CONTINUE
				m_i2cState = i2c_states::READ_LOW;
				//while((I2C0->STAT & 1)==0);	//	Waits MSTPENDING = 1
			}
			break;

		case i2c_states::READ_LOW:
			if(m_mstState == RECEIVE_RDY){					//	Receive Rdy
				m_data |= (((0xFF)&(m_i2c->MSTDAT)) << 8);	//	high_data_byte
				m_i2c->MSTCTL = (1 << 0); 					//	CONTINUE
				m_i2cState = i2c_states::READ_HIGH;
			}
			break;

		case i2c_states::READ_HIGH:
			m_i2c->MSTCTL = (1 << 2);	// STOP
			m_i2cState = i2c_states::STOP;
			break;

		case i2c_states::STOP:
			I2C::disableMasterInt();	//	Disable Master Pending Interrupt
			m_dataRdy = true;
			m_i2cState = i2c_states::IDLE;
			break;

		default:
			//	ERROR
			break;
	}
}

uint16_t I2C::getData(){ return m_data; }

bool I2C::isDataRdy(){ return m_dataRdy; }

void I2C0_IRQHandler(void){
	if(i2cInstance[0]) i2cInstance[0]->isrHandler();
}

void I2C1_IRQHandler(void){
	if(i2cInstance[1]) i2cInstance[1]->isrHandler();
}

void I2C2_IRQHandler(void){
	if(i2cInstance[2]) i2cInstance[2]->isrHandler();
}

void I2C3_IRQHandler(void){
	if(i2cInstance[3]) i2cInstance[3]->isrHandler();
}

I2C::~I2C(){}

