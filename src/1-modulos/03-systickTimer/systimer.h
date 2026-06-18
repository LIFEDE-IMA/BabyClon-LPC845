/*
 * systimer.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *	SysTick based timer
 */

#ifndef SYSTIMER_H_
#define SYSTIMER_H_

#include "LPC845.h"
#include "timedperipheral.h"

class SysTimer : public TimedPeripheral{
	public:
		enum shotType_t{
			SINGLE = 0,	//	Shoots timer just once, when expired its not shot again
			STRING		//	When expired, timer is auto-shot again (without usr request)
		};

		enum timeBase_t{
			T_MILI = 0,
			T_SEG,
			T_MIN
		};

	private:
		uint32_t m_time;
		uint32_t m_reload;
		shotType_t m_shot;
		timeBase_t m_base;
		void (*m_handler)(void);

		void baseToTicks(uint32_t time);	//	Sets 'time' as SysTick_Handler counter (when its called 'time' times, m_handler() is called)

	public:
		SysTimer(uint32_t time, void (*handler)(void), shotType_t shot, timeBase_t base = timeBase_t::T_SEG);		//	Constructor

		void startTimer();					//	Enables SysTick_Handler app
		void stopTimer();					//	SysTick_Handler is left with no effect
		void setTimer(uint32_t time);		//	Sets [m_reload]

		SysTimer& operator=(const uint32_t time);		//	Operator '=' overload
		SysTimer& operator+=(const uint32_t time);		//	Operator '+=' overload

		void handler(void);					//	SysTick ISR handler ( virtual from TemporizedPeripherals )

		friend void SysTick_Handler(void);	//	ISR handler

		~SysTimer();	//	Destructor
};



#endif /* SYSTIMER_H_ */
