/*
 * matrixkeyboard.h
 *
 *  Created on: 25 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle matrix keyboards.
 *  Check how KEY NUMBER is interpreted in .cpp
 */

#ifndef MATRIXKEYBOARD_H_
#define MATRIXKEYBOARD_H_

#include "timedperipheral.h"
#include "gpio.h"

class MatrixKeyboard : public TimedPeripheral{
	public:
		static const uint8_t NO_KEY = 0xFF;

	private:
		static const uint8_t MIN_SAMPLES = 5;

		uint8_t m_counter;

		Gpio *m_rowKeysArray;		//	Contains All Row Keys
		Gpio *m_columnKeysArray;	//	Contains All Column Keys
		uint8_t m_totalRowKeys;
		uint8_t m_totalColumnKeys;

		uint8_t m_currentKey;
		uint8_t m_previousKey;
		uint8_t m_keyBuffer;

		uint8_t sweepKeyboard(void);	//	Scans [m_keysArray] Searching an Active Key

	public:
		MatrixKeyboard(uint8_t totalRowKeys, Gpio *rowKeysArray, uint8_t totalColumnKeys, Gpio *columnKeysArray);	//	Constructor

		uint8_t getKey(void);	//	Returns Pressed Key

		void handler(void);	//	Virtual Method From TimedPeripheral (Called Every ~1ms)

		~MatrixKeyboard();	//	Destructor
};

#endif /* MATRIXKEYBOARD_H_ */
