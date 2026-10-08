/*
 * timedperipheral.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#include "timedperipheral.h"

TimedPeripheral **timedPeripheral_instances = nullptr;
uint8_t TimedPeripheral::timedPeripSources = 0;

static inline uint32_t timedPerCriticalEnter(void){	//	Saves Active Interrupts and Disables Them
	uint32_t primask;
	__asm volatile ("mrs %0, primask\n\tcpsid i" : "=r" (primask) : : "memory");
	return primask;
}

static inline void timedPerCriticalExit(uint32_t primask){	//	Restores Previous Active Interrupts
	__asm volatile ("msr primask, %0" : : "r" (primask) : "memory");
}

TimedPeripheral::TimedPeripheral(){
	//	Object Becomes "Visible" for Systick_Handler() here, but the Derived Class
	//	is Not Built Yet -> Members Not Initialized. If SysTick Fires Now and Calls
	//	handler() -> Crash
	//	Solution: Skip First Tick so Constructor has More Time (One Tick More)

	m_settling = true;

	TimedPeripheral **aux = new TimedPeripheral*[timedPeripSources + 1];	//	Adds new peripheral to the array

	for(uint8_t idx = 0; idx < timedPeripSources; idx++)
		aux[idx] = timedPeripheral_instances[idx];

	aux[timedPeripSources] = this;

	uint32_t primask = timedPerCriticalEnter();	//	Prevents SysTick from Firing Before Having Valid Sources' Array (HardFault)
	TimedPeripheral **old = timedPeripheral_instances;
	timedPeripheral_instances = aux;
	timedPeripSources++;
	timedPerCriticalExit(primask);	//	End of Critical Zone

	delete[] old;
}

void TimedPeripheral::tick(void){
	if(m_settling)
		m_settling = false;
	else
		handler();
}

TimedPeripheral::~TimedPeripheral(){
	uint8_t index;

	for(index = 0; index < timedPeripSources; index++)
		if(timedPeripheral_instances[index] == this)
			break;	//	Finds current instance position

	TimedPeripheral **aux = new TimedPeripheral *[timedPeripSources - 1];

	for(uint8_t i = 0; i < index; i++)
		aux[i] = timedPeripheral_instances[i];
	for(uint8_t i = index + 1; i < timedPeripSources; i++)
		aux[i - 1] = timedPeripheral_instances[i];	//	Deletes current instance from the array

	uint32_t primask = timedPerCriticalEnter();	//	Same Logic as Before
	TimedPeripheral **old = timedPeripheral_instances;
	timedPeripSources--;
	timedPeripheral_instances = aux;
	timedPerCriticalExit(primask);

	delete[] old;
}

