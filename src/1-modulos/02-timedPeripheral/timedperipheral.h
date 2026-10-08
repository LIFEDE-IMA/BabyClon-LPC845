/*
 * timedperipheral.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#ifndef TIMEDPERIPHERAL_H_
#define TIMEDPERIPHERAL_H_

#include <stdint.h>

class TimedPeripheral{
	private:
		volatile bool m_settling;	//	True from Construction till First Clk Tick (Object Still Being Built)

	public:
		TimedPeripheral();
		~TimedPeripheral();

		virtual void handler(void) = 0;

		void tick(void);	//	Called by SysTick_Handler() Instead of handler(): Skips Objects Not Built Yet

		static uint8_t timedPeripSources;

};

extern TimedPeripheral **timedPeripheral_instances;	//	Extern: used in other .cpp ( systick.cpp )

#endif /* TIMEDPERIPHERAL_H_ */
