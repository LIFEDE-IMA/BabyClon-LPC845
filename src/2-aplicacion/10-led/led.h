/*
 * led.h
 *
 *  Created on: 5 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#ifndef LED_H_
#define LED_H_

#include "gpio.h"
#include "timedperipheral.h"

class Led : public TimedPeripheral{
	public:
		enum timeBase_t : uint8_t{
			T_MILI,
			T_SEC,
			T_MIN
		};

	private:
		Gpio m_gpio;

		volatile bool m_blinkFlag;
		uint32_t m_time;
		uint32_t m_reload;

		void time2ticks(uint32_t time, timeBase_t timeBase);	//	Converts Time into Clk Ticks

	public:
		Led(bool port, uint8_t pin, Gpio::activeMode_t active = Gpio::activeMode_t::AM_HIGH);	//	Constructor

		void on(void);	//	Sets Led ON
		void off(void);	//	Sets Led OFF
		void blink(uint32_t speed = 1, timeBase_t timeBase = timeBase_t::T_SEC);	//	Toggles Led
		void steady(void);	//	Blink Off

		void handler(void);	//	Virtual Method from TimedPeripheral (called every ~1ms)

		~Led();	//	Destructor
};

#endif /* LED_H_ */
