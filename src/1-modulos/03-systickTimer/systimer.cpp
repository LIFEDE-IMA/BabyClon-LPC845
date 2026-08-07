/*
 * systimer.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *	SysTick based timer
 */

#include "systimer.h"

SysTimer::SysTimer(uint32_t time, shotType_t shot, timeBase_t base, void (*handler)(void)){	//	SYSCON (Cap. 8), SYSTICK (Cap. 25)
	m_base = base;
	SysTimer::baseToTicks(time);
	m_time = 0;
	m_handler = handler;
	m_shot = shot;
	m_timerExpiredFlag = false;
}

void SysTimer::baseToTicks(uint32_t time){
	switch(m_base){
		case timeBase_t::T_MILI:
			m_reload = time;
			break;

		case timeBase_t::T_SEG:
			m_reload = (time * 1000);
			break;

		case timeBase_t::T_MIN:
			m_reload = (time * 1000 * 60);
			break;

		default:
			break;
	}
}

void SysTimer::startTimer(){
	if(m_shot == shotType_t::STRING && m_handler == nullptr)
		return;

	if(m_reload != 0){
		m_time = m_reload;
		m_timerExpiredFlag = false;
	}
}

void SysTimer::stopTimer(){ m_time = 0; }

void SysTimer::setTimer(uint32_t time){
	SysTimer::stopTimer();
	SysTimer::baseToTicks(time);
	m_timerExpiredFlag = false;
}

bool SysTimer::singleTimerExpired(){
	if(m_shot == shotType_t::STRING)
		return false;

	if(m_timerExpiredFlag){
		m_timerExpiredFlag = false;
		return true;
	}
	return false;
}

bool SysTimer::isRunning() const{
	return (m_time != 0);
}

SysTimer& SysTimer::operator=(const uint32_t time){
	SysTimer::stopTimer();
	SysTimer::baseToTicks(time);
	m_timerExpiredFlag = false;

	return *this;
}

SysTimer& SysTimer::operator+=(const uint32_t time){
	SysTimer::stopTimer();
	m_timerExpiredFlag = false;

	uint32_t aux = m_reload;

	SysTimer::baseToTicks(time);

	m_reload += aux;

	return *this;
}

void SysTimer::handler(void){
	if(m_time){
		m_time--;

		if(!m_time){
			m_timerExpiredFlag = true;

			if(m_handler)
				m_handler();	//	User app

			if(m_shot == shotType_t::STRING)
				m_time = m_reload;	//	m_reload * 1ms = 'm_reload' s
		}
	}
}

SysTimer::~SysTimer(){}
