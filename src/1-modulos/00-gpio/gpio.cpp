#include "gpio.h"

//	IOCON-"fixed" access by port and pin

static const uint8_t iocon_index[2][32] = {
		{17, 11, 6, 5, 4, 3, 16, 15, 14, 13, 8, 7, 2, 1, 18, 10, 9, 0, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 50, 51, 35},
		{36, 37, 38, 41, 42, 43, 46, 49, 31, 32, 55, 54, 33, 34, 39, 40, 44, 45, 47, 48, 52, 53, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
};

//	SYSCON (Cap. 8), IOCON (Cap. 11), GPIO (Cap. 12)
Gpio::Gpio(bool port, uint8_t pin, dir_t dir, activeMode_t active) : m_port(port), m_pin(pin), m_dir(dir), m_active(active){
	m_error = error_t::E_NONE;

	if(m_port) SYSCON->SYSAHBCLKCTRL0 |= (1 << 20);
	else SYSCON->SYSAHBCLKCTRL0 |= (1 << 6);

	if(m_dir == dir_t::D_INPUT) GPIO->DIR[m_port] &= ~(1 << m_pin);
	else GPIO->DIR[m_port] |= (1 << m_pin);
}

void Gpio::setModeIN(inMode_t inMode){
	if(m_dir == dir_t::D_OUTPUT) GPIO->DIR[m_port] &= ~(1 << m_pin);	//	Set mode input

	IOCON->PIO[iocon_index[m_port][m_pin]] &= ~(0x3 << 3);		//	Clear bits
	IOCON->PIO[iocon_index[m_port][m_pin]] |= (inMode << 3);	//	Set bits

	m_mode = inMode;
}


void Gpio::setModeOUT(outMode_t outMode){
	if(m_dir == dir_t::D_INPUT) GPIO->DIR[m_port] |= (1 << m_pin);	//	Set mode output

	if(outMode == outMode_t::OM_NONE){
		IOCON->PIO[iocon_index[m_port][m_pin]] &= ~(1 << 10);
	}else{
		IOCON->PIO[iocon_index[m_port][m_pin]] |= (1 << 10);
	}

	m_mode = outMode;
}

void Gpio::togglePin(){ GPIO->PIN[m_port] ^= (1 << m_pin); }

void Gpio::setPin(){
	if(m_dir == dir_t::D_OUTPUT){
		if(m_active == activeMode_t::AM_HIGH) GPIO->PIN[m_port] |= (1 << m_pin);
		else GPIO->PIN[m_port] &= ~(1 << m_pin);
	}
}

void Gpio::clrPin(){
	if(m_dir == dir_t::D_OUTPUT){
		if(m_active == activeMode_t::AM_HIGH) GPIO->PIN[m_port] &= ~(1 << m_pin);
		else GPIO->PIN[m_port] |= (1 << m_pin);
	}
}

bool Gpio::getPin() const{ return ((GPIO->PIN[m_port] >> m_pin) & 0x1); }

bool Gpio::getPinState() const{
	if(m_active == activeMode_t::AM_HIGH)
		return ((GPIO->PIN[m_port] >> m_pin) & 0x1);

	if(m_active == activeMode_t::AM_LOW)
		return (~(GPIO->PIN[m_port] >> m_pin) & 0x1);

	return false;	//	Just to avoid the compiler warning
}

Gpio::~Gpio(){}


