/*
 * digitalinput.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#ifndef DIGITALINPUT_H_
#define DIGITALINPUT_H_

#include "timedperipheral.h"
#include "gpio.h"

class DigitalInput : public TimedPeripheral, Gpio{
	private:
		bool m_keyBuffer;
		uint8_t m_counter;
		Gpio::activeMode_t m_active;
		const uint8_t MIN_SAMPLES = 5;	//	Number of equal samples before we consider valid the signal

	public:
		DigitalInput(bool port, uint8_t pin, Gpio::activeMode_t active = Gpio::AM_HIGH);	//	Constructor

		bool getKey();		//	Returns the input pin state
		void handler();		//	Virtual function from TimedPeriheral (systick interrupt calls this method)

		~DigitalInput();	//	Destructor
};

#endif /* DIGITALINPUT_H_ */
