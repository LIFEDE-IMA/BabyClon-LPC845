/*
 * linealkeyboard.cpp
 *
 *  Created on: 25 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle simple keyboards
 */

#include "linealkeyboard.h"

LinealKeyboard::LinealKeyboard(uint8_t totalKeys, Gpio *keysArray){
	m_totalKeys = totalKeys;
	m_keysArray = keysArray;
	m_counter = 0;
	m_currentKey = LinealKeyboard::NO_KEY;
	m_previousKey = LinealKeyboard::NO_KEY;
	m_keyBuffer = LinealKeyboard::NO_KEY;
}

uint8_t LinealKeyboard::sweepKeyboard(void){
	uint8_t keyPressed = 0;

	for(keyPressed = 0; keyPressed < m_totalKeys; keyPressed++)
		if(m_keysArray[keyPressed].getPinState())	//	1: key pressed ( gpio solves activity logic )
			break;	//	Out of for()

	if(keyPressed == m_totalKeys)
		keyPressed = LinealKeyboard::NO_KEY;

	return keyPressed;
}


uint8_t LinealKeyboard::getKey(void){
	uint8_t ret = LinealKeyboard::NO_KEY;

	if(m_keyBuffer != LinealKeyboard::NO_KEY){	//	Key pressed
		ret = m_keyBuffer;
		m_keyBuffer = LinealKeyboard::NO_KEY ;
	}

	return ret;
}

void LinealKeyboard::handler(void){
	m_currentKey = LinealKeyboard::sweepKeyboard();

	if((m_currentKey == m_previousKey) && (m_counter < LinealKeyboard::MIN_SAMPLES)){
		m_counter++;
		if(m_counter == LinealKeyboard::MIN_SAMPLES) m_keyBuffer = m_currentKey;
	}else if(m_currentKey != m_previousKey)
		m_counter = 0;	//	We need MIN_SAMPLES CONSECUTIVE samples with no variation

	m_previousKey = m_currentKey;
}

LinealKeyboard::~LinealKeyboard(){}
