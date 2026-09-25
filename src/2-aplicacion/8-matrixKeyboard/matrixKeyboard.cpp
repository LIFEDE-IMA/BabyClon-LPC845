/*
 * matrixkeyboard.cpp
 *
 *  Created on: 25 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle matrix keyboards.
 */
/*
 * 	MATRIX:
 * 			     0	 	   1 		  2 	  ...		M
 *	0		(   key0	  key1	 	 key2	  ...	   keyM   )
 * 	1		(  keyM+1    keyM+2   	keyM+3	  ...    keyM+M+1 )
 * 	2		( keyM+M+2	keyM+M+3   keyM+M+4	  ...   keyM+M+M+2)
 * 	...			...		  ...		 ...	  ...	   ...
 * 	N		( 				key[totalM*N+(M+N)]				  )
 *
 */

#include "matrixkeyboard.h"

MatrixKeyboard::MatrixKeyboard(uint8_t totalRowKeys, Gpio *rowKeysArray, uint8_t totalColumnKeys, Gpio *columnKeysArray){
	m_totalRowKeys = totalRowKeys;
	m_rowKeysArray = rowKeysArray;
	m_totalColumnKeys = totalColumnKeys;
	m_columnKeysArray = columnKeysArray;
	m_counter = 0;
	m_currentKey = MatrixKeyboard::NO_KEY;
	m_previousKey = MatrixKeyboard::NO_KEY;
	m_keyBuffer = MatrixKeyboard::NO_KEY;
}

uint8_t MatrixKeyboard::sweepKeyboard(void){
	uint8_t keyPressed = MatrixKeyboard::NO_KEY;

	for(uint8_t column = 0; column < m_totalColumnKeys; column++){
		for(uint8_t idx = 0; idx < m_totalColumnKeys; idx++)
			m_columnKeysArray[idx].setPin();	//	All Columns: OFF ( Gpio Solves Activity Logic )

		m_columnKeysArray[column].clrPin();		//	Column [column]: ON ( Gpio Solves Activity Logic )

		uint8_t row = 0;
		for(row = 0; row < m_totalRowKeys; row++){
			if(m_rowKeysArray[row].getPinState() == 0)	//	Row [row]: ON ( Gpio Solves Activity Logic )
				break;	//	Out of for(row)
		}

		if(row != m_totalRowKeys)
			keyPressed = ((m_totalColumnKeys * row) + row + column);
	}

	return keyPressed;
}

uint8_t MatrixKeyboard::getKey(void){
	uint8_t ret = MatrixKeyboard::NO_KEY;

	if(m_keyBuffer != MatrixKeyboard::NO_KEY){	//	Key pressed
		ret = m_keyBuffer;
		m_keyBuffer = MatrixKeyboard::NO_KEY ;
	}

	return ret;
}

void MatrixKeyboard::handler(void){
	m_currentKey = MatrixKeyboard::sweepKeyboard();

	if((m_currentKey == m_previousKey) && (m_counter < MatrixKeyboard::MIN_SAMPLES)){
		m_counter++;
		if(m_counter == MatrixKeyboard::MIN_SAMPLES) m_keyBuffer = m_currentKey;
	}else if(m_currentKey != m_previousKey)
		m_counter = 0;	//	We need MIN_SAMPLES CONSECUTIVE samples with no variation

	m_previousKey = m_currentKey;
}

MatrixKeyboard::~MatrixKeyboard(){}

