/*
 * led.cpp
 *
 *  Created on: 5 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "led.h"

Led::Led(bool port, uint8_t pin, Gpio::activeMode_t active) : m_gpio(port, pin, Gpio::D_OUTPUT, active){
	m_blinkFlag = false;
	m_reload = 1000;
	m_time = m_reload;

	Led::off();
}

void Led::time2ticks(uint32_t time, timeBase_t timeBase){
	uint32_t msSpacing = (((SYSTICK->RVR + 1) * 1000) / FREQ_CLOCK);	//	Assuming Systick is Using Main_clk, this is the time (in ms) between virtual handler() calls

	switch(timeBase){
		case timeBase_t::T_MILI:
			m_reload = (time / msSpacing);
			break;

		case timeBase_t::T_SEC:
			m_reload = ((time * 1000) / msSpacing);
			break;

		case timeBase_t::T_MIN:
			m_reload = ((time * 1000 * 60) / msSpacing);
			break;

		default:
			break;
	}
}

void Led::on(void){
	m_blinkFlag = false;
	m_gpio.setPin();
}

void Led::off(void){
	m_blinkFlag = false;
	m_gpio.clrPin();
}

void Led::blink(uint32_t speed, timeBase_t timeBase){
	if(!speed)	return;

	Led::time2ticks(speed, timeBase);
	m_time = m_reload;

	m_blinkFlag = true;
}

void Led::steady(){ m_blinkFlag = false; }

void Led::handler(void){
	if(m_blinkFlag){
		if(m_time){
			m_time--;

			if(!m_time){
				m_gpio.togglePin();
				m_time = m_reload;
			}
		}
	}
}

Led::~Led(){}
