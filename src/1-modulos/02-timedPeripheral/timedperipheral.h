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
	public:
		TimedPeripheral();
		~TimedPeripheral();

		virtual void handler(void) = 0;
		static uint8_t timedPeripSources;

};

extern TimedPeripheral **timedPeripheral_instances;	//	Extern: used in other .cpp ( systick.cpp )

#endif /* TIMEDPERIPHERAL_H_ */
