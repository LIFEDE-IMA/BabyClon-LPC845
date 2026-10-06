/*
 * linealkeyboard.h
 *
 *  Created on: 25 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle simple keyboards
 */

#ifndef LINEALKEYBOARD_H_
#define LINEALKEYBOARD_H_

#include "timedperipheral.h"
#include "gpio.h"

class LinealKeyboard : public TimedPeripheral{
	public:
		static const uint8_t NO_KEY = 0xFF;

	private:
		static const uint8_t MIN_SAMPLES = 5;

		uint8_t m_counter;

		Gpio *m_keysArray;	//	Contains All Keyboard Keys
		uint8_t m_totalKeys;

		uint8_t m_currentKey;
		uint8_t m_previousKey;
		uint8_t m_keyBuffer;

		uint8_t sweepKeyboard(void);	//	Scans [m_keysArray] Searching an Active Key

	public:
		LinealKeyboard(uint8_t totalKeys, Gpio *keysArray);	//	Constructor

		uint8_t getKey(void);	//	Returns Pressed Key

		void handler(void);	//	Virtual Method From TimedPeripheral (Called Every ~1ms)

		~LinealKeyboard();	//	Destructor
};

#endif /* LINEALKEYBOARD_H_ */
