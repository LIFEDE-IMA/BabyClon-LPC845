/*
 * i2c.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "i2c.h"

I2C_Type* I2Cs[] = {I2C0, I2C1, I2C2, I2C3};

I2C *i2cInstance[I2C::TOTAL_I2C_SOURCES] = {nullptr, nullptr, nullptr, nullptr};

I2C::I2C(uint8_t i2cNumber, bool SDAport, uint8_t SDApin, bool SCLport, uint8_t SCLpin, uint32_t freqHz, uint16_t timeoutMs){
	m_i2c = (i2cNumber < I2C::TOTAL_I2C_SOURCES) ? I2Cs[i2cNumber] : nullptr;
	m_i2cNumber = i2cNumber;

	m_SDAport = (i2cNumber == 0) ? 0 : SDAport;
	m_SDApin = (i2cNumber == 0) ? 11 : SDApin;
	m_SCLport = (i2cNumber == 0) ? 0 : SCLport;
	m_SCLpin = (i2cNumber == 0) ? 10 : SCLpin;

	m_freqHz = 0;
	m_i2cState = i2cStates_t::ST_IDLE;
	m_i2cBusStatus = i2cStatus_t::BUS_IDLE;
	m_transferResult = i2cStatus_t::BUS_IDLE;

	m_slvAddr = 0;
	m_txBuff = nullptr;
	m_rxBuff = nullptr;
	m_txLen = m_rxLen = m_txIdx = m_rxIdx = 0;
	m_slvRegByte = 0;
	m_timeoutMs = timeoutMs;

	i2cInstance[i2cNumber] = this;

	I2C::init();
	I2C::setFreq(freqHz);
	I2C::enableNVIC_int();
}

void I2C::init(){	//	NVIC (Cap. 7), SYSCON (Cap. 8), SWITCH MATRIX (Cap. 10), IOCON (Cap. 11), I2C (Cap. 19)
	// Clk Enable
	SYSCON->FCLKSEL[(5 + m_i2cNumber)] = 0x1;	//	Clk source for I2Cx = main (30MHz)
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 7);		//	Enables SWM clk

	uint8_t sdaPIO = (m_SDApin + (m_SDAport * 32));
	uint8_t sclPIO = (m_SCLpin + (m_SCLport * 32));

	switch(m_i2cNumber){
		case 0:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 5);	//	Enables I2C0 clk
			// Reset I2C0 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 5);	//	RESET I2C0
			SYSCON->PRESETCTRL0 |= (1 << 5);	//	RELEASE RESET I2C0
			// Enable I2C pins (PIO0_10 SCL, PIO0_11 SDA) ~ Cant be others for I2C0
			SWM->PINENABLE0 &= ~((1 << 12) | (1 << 13));	//	Enables SDA Y SCL
			//	Config pins with IOCON register
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 18);		//	Enables IOCON Clk
			IOCON->PIO[8] &= ~((1 << 9) | (1 << 8));	// SCL: I2C
			IOCON->PIO[7] &= ~((1 << 9) | (1 << 8));	// SDA: I2C
			SYSCON->SYSAHBCLKCTRL0 &= ~(1 << 18);		//	Disables IOCON Clk to Save Power
			break;

		case 1:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 21);	//	Enables I2C1 clk
			// Reset I2C1 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 21);	//	RESET I2C1
			SYSCON->PRESETCTRL0 |= (1 << 21);	//	RELEASE RESET I2C1
			// Assign I2C pins
			SWM->PINASSIGN[9] &= ~((0xFF << 16) | (0xFF << 24));	//	Clears
			SWM->PINASSIGN[9] |= ((sdaPIO << 16) | (sclPIO << 24));	//	Enables SDA Y SCL
			break;

		case 2:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 22);	//	Enables I2C2 clk
			// Reset I2C2 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 22);	//	RESET I2C2
			SYSCON->PRESETCTRL0 |= (1 << 22);	//	RELEASE RESET I2C2
			// Assign I2C pins
			SWM->PINASSIGN[10] &= ~((0xFF << 0) | (0xFF << 8));		//	Clears
			SWM->PINASSIGN[10] |= ((sdaPIO << 0) | (sclPIO << 8));	//	Enables SDA Y SCL
			break;

		case 3:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 23);	//	Enables I2C3 clk
			// Reset I2C3 peripheral
			SYSCON->PRESETCTRL0 &= ~(1 << 23);	//	RESET I2C3
			SYSCON->PRESETCTRL0 |= (1 << 23);	//	RELEASE RESET I2C3
			// Assign I2C pins
			SWM->PINASSIGN[9] &= ~((0xFF << 16) | (0xFF << 24));		//	Clears
			SWM->PINASSIGN[10] |= ((sdaPIO << 16) | (sclPIO << 24));	//	Enables SDA Y SCL
			break;

		default:
			//	ERROR
			break;
	}

	m_i2c->CFG = (0 << 0);	//	Disables I2C
	m_i2c->INTENCLR |= (I2C::STAT_MSTPENDING | I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | (1 << 8) | (1 << 11) | (0x7 << 15) | (1 << 19) | (0x3 << 24));	//	Clears All Interrupts
}

bool I2C::setFreq(uint32_t freqHz){
	if((m_i2cState != i2cStates_t::ST_IDLE) || (freqHz == 0))
		return false;

	if(freqHz > I2C::MAX_FREQ_HZ)	freqHz = I2C::MAX_FREQ_HZ;

	/*
	 * 	Chap 19, Sec 19.7.1.1: Rate calculations
	 * 	SCL high = (CLKDIV + 1) * (MSTSCLHIGH + 2)	[function clocks]
	 * 	SCL low  = (CLKDIV + 1) * (MSTSCLLOW + 2)
	 * 	f_scl = clk / (SCL high + SCL low)
	 * 	DIVVAL (CLKDIV) must be >= 1, and for 400 kHz the clock after the divider must be <= 2 MHz.
	 *
	 * 'n' is the total number of divided clocks per SCL period (high + low). All the n are tried
	 * and the one that gives the highest frequency that doesn't exceed the requested one is used.
	 * Above 100 kHz (Fast mode) low time is longer than high time and the minimum times are
	 * checked: tLOW >= 1.3 us, tHIGH >= 0.6 us.
	 */

	bool fast = (freqHz > 100000);
	uint32_t bestFreq = 0, bestDiv = 0, bestLowCyc = 0, bestHighCyc = 0;


	for(uint8_t n = 4; n <= 16; n++){
		uint32_t div = ((((uint32_t)FREQ_CLOCK) + (freqHz * n) - 1) / (freqHz * n));
		if(div < 2)	div = 2;
		if(div > 0x10000)	continue;	//	Next For Cycle

		if(fast && ((((uint32_t)FREQ_CLOCK) / div) > 2000000))	continue;	//	Next For Cycle

		uint32_t low = fast ? (((3 * n) + 4) / 5) : ((n + 1) / 2);	//	If fast -> Low ~60%
		uint32_t high = (n - low);
		if((low < 2) || (high < 2)	|| (low > 9) || (high > 9))	continue;

		if(fast){
			if((low * div * 10000) < (13 * (((uint32_t)FREQ_CLOCK) / 1000)))	continue;	//	t_low >= 1.3us
			if((high * div * 10000) < (6 * (((uint32_t)FREQ_CLOCK) / 1000)))	continue;	//	t_high >= 0.6us

		}

		uint32_t actualFreq = ((uint32_t)FREQ_CLOCK / (div * n));
		if(actualFreq > bestFreq){
			bestFreq = actualFreq;
			bestDiv = div;
			bestLowCyc = low;
			bestHighCyc = high;
		}
	}

	if(!bestFreq)	return false;

	m_i2c->CFG &= ~(1 << 0);	//	I2C Master Disabled
	m_i2c->CLKDIV = (bestDiv - 1);
	m_i2c->MSTTIME &= ~(0x7F);	//	Clears register
	m_i2c->MSTTIME |= (((bestHighCyc - 2) << 4) | ((bestLowCyc - 2) << 0));
	m_i2c->CFG |= (1 << 0);		//	I2C Master Enabled

	m_freqHz = bestFreq;
	m_baseClkHz = ((uint32_t)FREQ_CLOCK / bestDiv);

	I2C::setTimeout();

	return true;
}

uint32_t I2C::getFreq(void) const{ return m_freqHz; }

void I2C::setTimeout(void){
	m_i2c->CFG &= ~(1 << 3);	//	Disable TIMEOUTEN

	if(!m_timeoutMs)	return;

	/* Chap 19, Sec 19.6.5:
	 * time-out = (TO + 1) * 16 clocks, TO is 12 bits (TOMIN, the low 4 bits, are always 0xF).
	 * Event time-out: time between bus events (START, SCL edges, STOP) while the bus is busy.
	 */

	uint32_t clocks = ((m_baseClkHz / 1000) * m_timeoutMs);
	uint32_t to = ((clocks + 0xF) / 16);
	if(to < 1) to = 1;
	if(to > 0x1000) to = 0x1000;

	m_i2c->TIMEOUT &= ~(0xFFF << 4);	//	Clears Register
	m_i2c->TIMEOUT |= ((to - 1) << 4);
	m_i2c->CFG |= (1 << 3);	//	Enable TIMEOUTEN
}

bool I2C::setTimeout(uint16_t ms){
	if(m_i2cState != i2cStates_t::ST_IDLE)	return false;

	m_timeoutMs = ms;
	I2C::setTimeout();
	return true;
}

void I2C::enableNVIC_int(void){
	switch(m_i2cNumber){
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

void I2C::disableNVIC_int(void){
	switch(m_i2cNumber){
		case 0:
			NVIC->ICER[0] = (1 << I2C0_IRQn);	//	Disables NVIC interrupt
			break;

		case 1:
			NVIC->ICER[0] = (1 << I2C1_IRQn);	//	Disables NVIC interrupt
			break;

		case 2:
			NVIC->ICER[0] = (1 << I2C2_IRQn);	//	Disables NVIC interrupt
			break;

		case 3:
			NVIC->ICER[0] = (1 << I2C3_IRQn);	//	Disables NVIC interrupt
			break;

		default:
			//	ERROR
			break;
	}
}

void I2C::enableMasterInt(void){
	uint8_t timeoutINT = (m_timeoutMs != 0) ? I2C::STAT_EVENTTIMEOUT : 0;
	m_i2c->INTENSET |= (I2C::STAT_MSTPENDING | I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | timeoutINT);	//	Enable Master Pending, Master Arbitration Loss, Master Start/Stop Error and Timeout Interrupt
}

void I2C::disableMasterInt(void){
	m_i2c->INTENCLR = (I2C::STAT_MSTPENDING | I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | I2C::STAT_EVENTTIMEOUT | I2C::STAT_SCLTIMEOUT);	//	Disable Master Pending, Master Arbitration Loss, Master Start/Stop Error and Timeout Interrupt
}

bool I2C::transfer(uint8_t slvAddress, const uint8_t *tx, uint16_t txLen, uint8_t *rx, uint16_t rxLen){
	if(m_i2cState != i2cStates_t::ST_IDLE)	return false;	//	Driver Busy
	if(slvAddress > 0x7F)	return false;	//	I2C Slaves Have 7 bit Addresses
	if(((txLen > 0) && (tx == nullptr)) || ((rxLen > 0) && (rx == nullptr)))	return false;

	uint32_t stat = m_i2c->STAT;
	if(!(stat & I2C::STAT_MSTPENDING) || (((stat >> 1) & 0x7) != I2C::STAT_MST_IDLE))
		return false;	//	HW is NOT IDLE

	m_slvAddr = slvAddress;
	m_txBuff = tx;
	m_txLen = txLen;
	m_txIdx = 0;
	m_rxBuff = rx;
	m_rxLen = rxLen;
	m_rxIdx = 0;
	m_transferResult = i2cStatus_t::BUS_IDLE;
	m_i2cBusStatus = i2cStatus_t::BUS_BUSY;

	//	Clear Any Previous Error
	m_i2c->STAT |= (I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | I2C::STAT_EVENTTIMEOUT | I2C::STAT_SCLTIMEOUT);

	if((txLen > 0) || (rxLen == 0)){	//	Write Or Address Only
		m_i2cState = i2cStates_t::ST_TX;
		m_i2c->MSTDAT = ((m_slvAddr << 1) | 0);	//	Slv Addr + W
	}else{	//	Plain Read
		m_i2cState = i2cStates_t::ST_RX;
		m_i2c->MSTDAT = ((m_slvAddr << 1) | 1);	//	Slv Addr + R
	}

	m_i2c->MSTCTL = I2C::MSTCTL_START;	//	START

	I2C::enableMasterInt();

	return true;
}

bool I2C::write(uint8_t slvAddress, const uint8_t *tx, uint16_t txLen){
	return I2C::transfer(slvAddress, tx, txLen, nullptr, 0);
}

bool I2C::read(uint8_t slvAddress, uint8_t *rx, uint16_t rxLen){
	return I2C::transfer(slvAddress, nullptr, 0, rx, rxLen);
}

bool I2C::readRegister(uint8_t slvAddress, uint8_t regAddress, uint8_t *rx, uint16_t rxLen){
	if(m_i2cState != i2cStates_t::ST_IDLE)	return false;	//	[m_slvRegByte] in Use
	m_slvRegByte = regAddress;
	return I2C::transfer(slvAddress, &m_slvRegByte, 1, rx, rxLen);
}

bool I2C::presence(uint8_t slvAddress){
	return I2C::transfer(slvAddress, nullptr, 0, nullptr, 0);
}

void I2C::startStop(i2cStatus_t result){
	m_transferResult = result;
	m_i2c->MSTCTL = I2C::MSTCTL_STOP;	//	STOP
	m_i2cState = i2cStates_t::ST_STOP;
}

void I2C::finish(i2cStatus_t status){
	I2C::disableMasterInt();

	m_i2cBusStatus = status;
	m_i2cState = i2cStates_t::ST_IDLE;
}

void I2C::abort(i2cStatus_t status){
	if(m_i2cState == i2cStates_t::ST_IDLE)	return;

	I2C::disableMasterInt();
	m_i2c->CFG &= ~(1 << 0);	//	I2C Master Disabled
	m_i2c->CFG |= (1 << 0);		//	I2C Master Enabled
	m_i2c->STAT |= (I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | I2C::STAT_EVENTTIMEOUT | I2C::STAT_SCLTIMEOUT);	//	Clear Error Flags

	I2C::finish(status);
}

void I2C::abort(void){ I2C::abort(i2cStatus_t::ABORTED); }

bool I2C::isBusy(void) const{ return (m_i2cBusStatus == i2cStatus_t::BUS_BUSY); }

bool I2C::isDone(void) const{ return (m_i2cBusStatus == i2cStatus_t::BUS_OP_DONE); }

bool I2C::hasError(void) const{
	return ((m_i2cBusStatus >= i2cStatus_t::ERR_NACK_ADDR) && (m_i2cBusStatus <= i2cStatus_t::ERR_TIMEOUT));
}

I2C::i2cStatus_t I2C::getStatus(void) const{ return m_i2cBusStatus; }

uint16_t I2C::getBytesWritten(void) const{ return m_txIdx; }

uint16_t I2C::getBytesRead(void) const{ return m_rxIdx; }

void I2C::isrHandler(){
	if(m_i2cState == i2cStates_t::ST_IDLE){	//	IRQ Not Wanted
		m_i2c->STAT |= (I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | I2C::STAT_EVENTTIMEOUT | I2C::STAT_SCLTIMEOUT);	//	Clear Error Flags
		I2C::disableMasterInt();
		return;
	}

	uint32_t stat = m_i2c->STAT;

	if(stat & (I2C::STAT_MSTARBLOSS | I2C::STAT_MSTMSTPERR | I2C::STAT_EVENTTIMEOUT)){	//	HW ERROR
		i2cStatus_t error = i2cStatus_t::ERR_BUS;

		if(stat & I2C::STAT_EVENTTIMEOUT)	error = i2cStatus_t::ERR_TIMEOUT;
		else if(stat & I2C::STAT_MSTARBLOSS)	error = i2cStatus_t::ERR_ARB_LOST;

		I2C::abort(error);
		return;
	}

	if(!(stat & I2C::STAT_MSTPENDING))	//	No Master Pending
		return;

	uint8_t mstState = ((stat >> 1) & 0x7);

	if((m_i2cState != i2cStates_t::ST_STOP) && ((mstState == I2C::STAT_MST_NACK_ADDR) || (mstState == I2C::STAT_MST_NACK_DATA))){	//	NACK
		i2cStatus_t stopStatus = (mstState == I2C::STAT_MST_NACK_ADDR) ? i2cStatus_t::ERR_NACK_ADDR : i2cStatus_t::ERR_NACK_DATA;
		I2C::startStop(stopStatus);
		return;
	}

	switch(m_i2cState){
		case i2cStates_t::ST_TX:
			if(mstState != I2C::STAT_MST_TX_RDY){//	Transmit Rdy
				I2C::startStop(i2cStatus_t::ERR_BUS);
			}
			else if(m_txIdx < m_txLen){
				m_i2c->MSTDAT = m_txBuff[m_txIdx];		//	Sends Data
				m_txIdx++;
				m_i2c->MSTCTL = I2C::MSTCTL_CONTINUE;	//	CONTINUE
			}
			else if(m_rxLen > 0){	// Write, Repeated Start, Read (Write Done)
				m_i2c->MSTDAT = ((m_slvAddr << 1) | 1);	//	Slave address + read
				m_i2c->MSTCTL = I2C::MSTCTL_START;		//	REPEATED START
				m_i2cState = i2cStates_t::ST_RX;
			}
			else{	//	Nothing Else Pending
				I2C::startStop(i2cStatus_t::BUS_OP_DONE);
			}
			break;

		case i2cStates_t::ST_RX:
			if(mstState != I2C::STAT_MST_RX_RDY){	//	Receive Rdy
				I2C::startStop(i2cStatus_t::ERR_BUS);
			}else{
				m_rxBuff[m_rxIdx] = (uint8_t)(m_i2c->MSTDAT & 0xFF);	//	Low Data Byte
				m_rxIdx++;

				if(m_rxIdx >= m_rxLen)
					I2C::startStop(i2cStatus_t::BUS_OP_DONE);	//	Last Byte
				else
					m_i2c->MSTCTL = I2C::MSTCTL_CONTINUE; 		//	CONTINUE
			}
			break;

		case i2cStates_t::ST_STOP:
			I2C::finish(m_transferResult);
			break;

		default:
			//	ERROR
			break;
	}
}

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

I2C::~I2C(){
	I2C::disableMasterInt();
	I2C::disableNVIC_int();
	m_i2c->CFG = 0;
	i2cInstance[m_i2cNumber] = nullptr;
}

