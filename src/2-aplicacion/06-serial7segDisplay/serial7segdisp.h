/*
 * serial7segdisp.h
 *
 *  Created on: 22 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle 74HC595D based 7-segments display
 */

#ifndef SERIAL7SEGDISP_H_
#define SERIAL7SEGDISP_H_

#include "spi.h"
#include "timedperipheral.h"

class Serial7segDisp : public SpiSlave, TimedPeripheral{
	public:
		enum segActiveMode_t : uint8_t{
			SEGMENTS_ACTIVE_LOW = 0,
			SEGMENTS_ACTIVE_HIGH
		};

		enum digSelActiveMode_t : uint8_t{
			DIGIT_SELECT_ACTIVE_LOW = 0,
			DIGIT_SELECT_ACTIVE_HIGH
		};

		static const uint8_t TOTAL_DISP_DIGITS = 4;

		static const uint8_t VALUE_SEG_CODE_TABLE[10];	//	Value Coded in Seg Code

	private:
		static const uint8_t DP_CODE = 0x80;
		static const uint8_t MINUS_CODE = 0x40;
		static const uint8_t BLANK_CODE = 0x00;

		segActiveMode_t m_segmentActiveMode;
		digSelActiveMode_t m_digitSelActiveMode;

		uint8_t m_digitPattern[Serial7segDisp::TOTAL_DISP_DIGITS];	//	Contains the Active Segments of Each Digit
		uint8_t m_txBuffer[2];
		uint8_t m_currentDigit;

		static uint8_t segmentCodeFor(uint8_t value);	//	Returns Code for any Value 0-9
		static uint8_t countDigits(uint32_t value);		//	Returns Digits Amount Needed to Represent [value]
		void showOverflow(void);						//	Sets the Display to "- - - -" When Tried to Set Digits > TOTAL_DISP_DIGITS
		void setValueCore(uint32_t integer, bool isFractional, uint8_t frac, bool isNegative);	//	Sets Segments Needed to Represent int32_t value

	public:
		Serial7segDisp(bool portRCLK, uint8_t pinRCLK, Spi &spi, segActiveMode_t segActMode = segActiveMode_t::SEGMENTS_ACTIVE_LOW, digSelActiveMode_t digSelActMode = digSelActiveMode_t::DIGIT_SELECT_ACTIVE_HIGH);		//	Constructor

		void setDigitRaw(uint8_t digit, uint8_t segment);					//	Sets Segment 0-7 from Digit [digit]
		void setDigitValue(uint8_t digit, uint8_t value, bool dp = false);	//	Sets Value 0-9 in Digit [digit] with Optional Decimal Point
		void setDigitBlank(uint8_t digit);									//	Keeps Digit [digit] Off

		void setValue(float value);		//	Sets display value

		void handler(void);		//	Updates Display Every ~1ms ( virtual from TemporizedPeripherals )

		~Serial7segDisp();		//	Destructor
};

#endif /* SERIAL7SEGDISP_H_ */
