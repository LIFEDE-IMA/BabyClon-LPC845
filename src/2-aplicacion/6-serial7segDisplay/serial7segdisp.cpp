/*
 * serial7segdisp.cpp
 *
 *  Created on: 22 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle 74HC595D based 7-segments display
 */

#include "serial7segdisp.h"

const uint8_t Serial7segDisp::VALUE_SEG_CODE_TABLE[10] = {
	0b0111111,	//	0
	0b0000110,	//	1
	0b1011011,	//	2
	0b1001111,	//	3
	0b1100110,	//	4
	0b1101101,	//	5
	0b1111101,	//	6
	0b0000111,	//	7
	0b1111111,	//	8
	0b1101111	//	9
};

Serial7segDisp::Serial7segDisp(bool portRCLK, uint8_t pinRCLK, Spi &spi, segActiveMode_t segActMode, digSelActiveMode_t digSelActMode) : SpiSlave(portRCLK, pinRCLK, spi, Spi::SPI_SLAVE_SELECT_ACTIVE_LOW){
	m_currentDigit = 0;

	for(uint8_t idx = 0; idx < Serial7segDisp::TOTAL_DISP_DIGITS; idx++)	m_digitPattern[idx] = Serial7segDisp::BLANK_CODE;

	m_txBuffer[0] = 0;
	m_txBuffer[1] = 0;

	m_segmentActiveMode = segActMode;
	m_digitSelActiveMode = digSelActMode;
}

uint8_t Serial7segDisp::segmentCodeFor(uint8_t value){
	if(value < 10)
		return (Serial7segDisp::VALUE_SEG_CODE_TABLE[value]);
	else
		return Serial7segDisp::BLANK_CODE;
}

void Serial7segDisp::setDigitRaw(uint8_t digit, uint8_t segment){
	if(digit < Serial7segDisp::TOTAL_DISP_DIGITS)
		m_digitPattern[digit] = segment;
}

void Serial7segDisp::setDigitValue(uint8_t digit, uint8_t value, bool dp){
	if(digit >= Serial7segDisp::TOTAL_DISP_DIGITS)	return;

	uint8_t digitPattern = Serial7segDisp::segmentCodeFor(value);

	if(dp)	digitPattern |= Serial7segDisp::DP_CODE;

	m_digitPattern[digit] = digitPattern;
}

void Serial7segDisp::setDigitBlank(uint8_t digit){
	if(digit < Serial7segDisp::TOTAL_DISP_DIGITS)
		m_digitPattern[digit] = Serial7segDisp::BLANK_CODE;
}

uint8_t Serial7segDisp::countDigits(uint32_t value){
	uint8_t count = 1;
	while(value >= 10){
		value /= 10;
		count++;
	}
	return count;
}

void Serial7segDisp::showOverflow(void){
	for(uint8_t dig = 0; dig < Serial7segDisp::TOTAL_DISP_DIGITS; dig++)
		Serial7segDisp::setDigitRaw(dig, Serial7segDisp::MINUS_CODE);
}

void Serial7segDisp::setValueCore(uint32_t integer, bool isFractional, uint8_t frac, bool isNegative){
	uint8_t integerDigits = Serial7segDisp::countDigits(integer);
	uint8_t totalDigits = (integerDigits + (isFractional ? 1 : 0) + (isNegative ? 1 : 0));	//	Only 1 Decimal

	if(totalDigits > Serial7segDisp::TOTAL_DISP_DIGITS){
		Serial7segDisp::showOverflow();
		return;
	}

	uint8_t freeSlots = (Serial7segDisp::TOTAL_DISP_DIGITS - totalDigits);	//	Blanks Will Be 0 ... (freeSlots - 1)

	for(uint8_t digit = 0; digit < freeSlots; digit++)
		Serial7segDisp::setDigitBlank(digit);

	if(isNegative){
		Serial7segDisp::setDigitRaw(freeSlots, Serial7segDisp::MINUS_CODE);	//	Set Minus Segment After Last Blank Digit
		freeSlots++;
	}

	uint8_t lastIntegerSlot = (freeSlots + integerDigits - 1);	//	Display's Digit Which Contains Last Integer Digit
	uint32_t remainingInteger = integer;

	for(uint8_t currentSlot = 0; currentSlot < integerDigits; currentSlot++){
		uint8_t digitValue = (uint8_t)(remainingInteger % 10);
		remainingInteger /= 10;
		uint8_t digit = (lastIntegerSlot - currentSlot);
		bool dp = (isFractional && (currentSlot == 0));	//	0 is the LSB from integer so its the only digit that can have dp
		Serial7segDisp::setDigitValue(digit, digitValue, dp);
	}

	if(isFractional){
		Serial7segDisp::setDigitValue((lastIntegerSlot + 1), frac, false);	//	Sets Decimal Digit
	}
}

void Serial7segDisp::setValue(float value){
	bool isNegative = (value < 0.0f);
	float magnitude = (isNegative ? -value : value);

	uint8_t availableDigits = (isNegative ? (Serial7segDisp::TOTAL_DISP_DIGITS - 1) : Serial7segDisp::TOTAL_DISP_DIGITS);

	uint32_t round = (uint32_t)((magnitude * 10.0f) + 0.5f);	//	Round With Only 1 Decimal
	uint32_t integer = (round / 10);
	uint8_t fracDigit = (uint8_t)(round % 10);
	bool isFractional = (fracDigit != 0);

	if(isFractional){
		uint8_t totalDigits = (Serial7segDisp::countDigits(integer) + 1);	//	Integer + decimal

		if(totalDigits <= availableDigits){
			Serial7segDisp::setValueCore(integer, true, fracDigit, isNegative);
			return;
		}
	}
	//	Integer + Decimal > total_digits or Value Is Not Fractional
	round = (uint32_t)(magnitude + 0.5f);
	uint8_t totalDigits = Serial7segDisp::countDigits(round);

	if(totalDigits > availableDigits){
		Serial7segDisp::showOverflow();
		return;
	}

	Serial7segDisp::setValueCore(round, false, 0, isNegative);
}

void Serial7segDisp::handler(void){
	uint8_t physicalDigit = (Serial7segDisp::TOTAL_DISP_DIGITS - 1 - m_currentDigit);	//	Mirrored
	uint8_t selectMask = (uint8_t)(1 << physicalDigit);
	uint8_t segmentsOut = m_digitPattern[m_currentDigit];

	if(m_digitSelActiveMode == digSelActiveMode_t::DIGIT_SELECT_ACTIVE_LOW)
		selectMask = ~selectMask;

	if(m_segmentActiveMode == segActiveMode_t::SEGMENTS_ACTIVE_LOW)
		segmentsOut = ~segmentsOut;

	m_txBuffer[0] = segmentsOut;	//	Check integrated circuit
	m_txBuffer[1] = selectMask;		//	to know which first [0] / [1]

	Transmit(m_txBuffer, nullptr, 2, nullptr);

	m_currentDigit++;
	if(m_currentDigit >= Serial7segDisp::TOTAL_DISP_DIGITS)	m_currentDigit = 0;
}

Serial7segDisp::~Serial7segDisp(){}
