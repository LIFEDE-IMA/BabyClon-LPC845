/*
 * digitalinput.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#include "digitalinput.h"

DigitalInput::DigitalInput(bool port, uint8_t pin, Gpio::activeMode_t active) :
Gpio(port, pin, Gpio::D_INPUT, active), m_active(active){
	m_keyBuffer = 0;
	m_counter = 0;
}

bool DigitalInput::getKey(){
	if(m_keyBuffer){	//	Key pressed
		m_keyBuffer = false;
		return !m_keyBuffer;
	}
	return m_keyBuffer;
}

void DigitalInput::handler(){
	bool currentState;
	static bool previousState = 1;	//	Keeps its value through function calls

	currentState = Gpio::getPinState();	//	1: key pressed ( gpio solves activity logic )

	if((currentState == previousState) && (m_counter < MIN_SAMPLES)){
		m_counter++;
		if(m_counter == MIN_SAMPLES) m_keyBuffer = currentState;
	}else if(currentState != previousState)
		m_counter = 0;	//	We need MIN_SAMPLES CONSECUTIVE samples with no variation

	previousState = currentState;
}

DigitalInput::~DigitalInput(){}
