/*
 * sctimer.cpp
 *
 *  Created on: 15 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 */

#include "sctimer.h"

//	NVIC (Cap. 7), SYSCON (Cap. 8), SWM (Cap. 10), INPUT MUX (Cap. 14), SCTIMER (Cap. 21)
SCTimer::SCTimer(sctOpMode_t opMode, bool setInputTrigger, bool setOutputTrigger){
	m_sctOpMode = opMode;
	m_sctClkMode = sctClkMode_t::sctCLKMODE_SYSTEM;		//	Default

	m_sctInputTriggerConfigured_flag = setInputTrigger;
	m_sctOutputTriggerConfigured_flag = setOutputTrigger;

	m_sctClkSel = sctClkSel_t::sctCKSEL_RISING_EDGE_INPUT0;	//	Default
	m_sctReloadL = false;	//	Default
	m_sctReloadH = true;	//	Default
	m_sctLimitL = false;	//	Default
	m_sctLimitH = false;	//	Default

	if(m_sctInputTriggerConfigured_flag){
		m_sctInputNumber = InMux::SCT_INPUT_NUMBER_t::INPUT_0;	//	Default
		m_sctInputSource = InMux::SCT_INMUX_SOURCE_t::SCT_PIN0;	//	Default
		m_sctInputPort = 0;		//	Default
		m_sctInputPin = 1;		//	Default
	}

	if(m_sctOutputTriggerConfigured_flag){
		m_sctOutputNumber = outputNumber_t::sctOUTPUT_0;	//	Default
		m_sctOutputType = outputType_t::sctOUTPUTtype_PIN;	//	Default
		m_sctOutputPort = 0;	//	Default
		m_sctOutputPin = 0;		//	Default
	}

	//	ALL "DEFAULT" PARAMS CAN BE MODIFIED BY CALLING configXXXX() METHODS

	SCTimer::disableInt();	//	Disable NVIC Interrupt
}

void SCTimer::configCLK(sctClkMode_t ClkMode, sctClkSel_t clkSel){
	m_sctClkMode = ClkMode;
	m_sctClkSel = clkSel;
}
void SCTimer::configReload(bool reloadL, bool reloadH){
	m_sctReloadL = reloadL;

	if(m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE)
		m_sctReloadH = true;	//	Dont charge NORELOAD_H register
	else
		m_sctReloadH = reloadH;
}

void SCTimer::configAutoLimit(bool limitL, bool limitH){
	m_sctLimitL = limitL;

	if(m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE)
		m_sctLimitH = false;	//	Dont charge LIMIT_H register
	else
		m_sctLimitH = limitH;
}

void SCTimer::configInput(InMux::SCT_INPUT_NUMBER_t inputNumber, InMux::SCT_INMUX_SOURCE_t inputSource, bool inputPort, uint8_t inputPin){
	m_sctInputNumber = inputNumber;
	m_sctInputSource = inputSource;
	m_sctInputPort = inputPort;
	m_sctInputPin = inputPin;
	m_sctInputTriggerConfigured_flag = true;
}

void SCTimer::configOutput(outputNumber_t outputNumber, outputType_t outputType, bool outputPort, uint8_t outputPin){
	m_sctOutputNumber = outputNumber;
	m_sctOutputType = outputType;
	m_sctOutputPort = outputPort;
	m_sctOutputPin = outputPin;
	m_sctOutputTriggerConfigured_flag = true;
}


void SCTimer::init(void){
	SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 8));	//	Enable Clk for SWM and SCT
	//	RESET SCT
	SYSCON->PRESETCTRL0 &= ~(1 << 8);	//	Assert Reset
	SYSCON->PRESETCTRL0 |= (1 << 8);	//	Clear Reset

	//	Connect SCT INPUTS with INMUX and SWM
	if(m_sctInputTriggerConfigured_flag)	SCTimer::inputConfig();

	//	Connect SCT OUTPUTS with SWM
	if(m_sctOutputTriggerConfigured_flag)	SCTimer::outputConfig();

	//	SCT CONFIG
	SCT->CFG = (m_sctOpMode << 0);	//	Sets Operation Mode in UNIFIED or in 2 registers of 16bits
	SCT->CFG |= ((m_sctClkMode << 1)	|	//	CLKMODE
				 (m_sctClkSel << 3)		|	//	CKSEL
				 (!m_sctReloadL << 7)	|	//	NORELOAD_L
				 (!m_sctReloadH << 8)	|	//	NORELOAD_H
				 //(inBit << 9)			|	//	INSYNC Sync for Input 0-3 (CFG[12:9] bit12 = input3 ... bit9 = input0)
				 (m_sctLimitL << 17)	|	//	AUTOLIMIT_L
				 (m_sctLimitH << 18));		//	AUTOLIMIT_H

}

void SCTimer::inputConfig(void){
	InMux::setSCT_INMUX(m_sctInputNumber, m_sctInputSource);
	if(m_sctInputSource <= InMux::SCT_INMUX_SOURCE_t::SCT_PIN3){	//	Input Source is a GPIO
		uint8_t inputGPIO = (m_sctInputPin + (m_sctInputPort * 32));
		if(m_sctInputSource == InMux::SCT_INMUX_SOURCE_t::SCT_PIN0){
			SWM->PINASSIGN[6] |= (inputGPIO << 24);		//	SCT Input PIN 0
		}else{
			uint8_t shift = ((m_sctInputSource - 1) * 8);
			SWM->PINASSIGN[7] |= (inputGPIO << shift);	//	SCT Input PIN 1-3
		}
	}
}

void SCTimer::outputConfig(void){
	uint8_t outputGPIO = (m_sctOutputPin + (m_sctOutputPort * 32));

	if(m_sctOutputNumber == outputNumber_t::sctOUTPUT_0){
		SWM->PINASSIGN[7] |= (outputGPIO << 24);	//	SCT Output 0
	}else if(m_sctOutputNumber <= outputNumber_t::sctOUTPUT_4){
		if(m_sctOutputType == outputType_t::sctOUTPUTtype_PIN){	//	Output is selected as GPIO
			uint8_t shift = ((m_sctOutputNumber - 1) * 8);
			SWM->PINASSIGN[8] |= (outputGPIO << shift);	//	SCT Output 1-4
		}else if(m_sctOutputNumber >= outputNumber_t::sctOUTPUT_3){	//	Only SCT Output 3-4 can be set as ADC Input Trigger
			SCTimer::setOutputADC(m_sctOutputType);	//	SCT Output ADC0-11
		}
	}else{
		uint8_t shift = ((m_sctOutputNumber - 5) * 8);
		SWM->PINASSIGN[9] |= (outputGPIO << shift);	//	SCT Output 5-9
	}
}

void SCTimer::setOutputADC(outputType_t outputADC){
	uint8_t shift = ((m_sctOutputNumber - 1) * 8);

	switch(outputADC){
		case outputType_t::sctOUTPUTtype_ADC0_TRIGGER:
			SWM->PINASSIGN[8] |= (7 << shift);	//	PIO0_07
			break;

		case outputType_t::sctOUTPUTtype_ADC1_TRIGGER:
			SWM->PINASSIGN[8] |= (6 << shift);	//	PIO0_06
			break;

		case outputType_t::sctOUTPUTtype_ADC2_TRIGGER:
			SWM->PINASSIGN[8] |= (14 << shift);	//	PIO0_14
			break;

		case outputType_t::sctOUTPUTtype_ADC3_TRIGGER:
			SWM->PINASSIGN[8] |= (23 << shift);	//	PIO0_23
			break;

		case outputType_t::sctOUTPUTtype_ADC4_TRIGGER:
			SWM->PINASSIGN[8] |= (22 << shift);	//	PIO0_22
			break;

		case outputType_t::sctOUTPUTtype_ADC5_TRIGGER:
			SWM->PINASSIGN[8] |= (21 << shift);	//	PIO0_21
			break;

		case outputType_t::sctOUTPUTtype_ADC6_TRIGGER:
			SWM->PINASSIGN[8] |= (20 << shift);	//	PIO0_20
			break;

		case outputType_t::sctOUTPUTtype_ADC7_TRIGGER:
			SWM->PINASSIGN[8] |= (19 << shift);	//	PIO0_19
			break;

		case outputType_t::sctOUTPUTtype_ADC8_TRIGGER:
			SWM->PINASSIGN[8] |= (18 << shift);	//	PIO0_18
			break;

		case outputType_t::sctOUTPUTtype_ADC9_TRIGGER:
			SWM->PINASSIGN[8] |= (17 << shift);	//	PIO0_17
			break;

		case outputType_t::sctOUTPUTtype_ADC10_TRIGGER:
			SWM->PINASSIGN[8] |= (13 << shift);	//	PIO0_13
			break;

		case outputType_t::sctOUTPUTtype_ADC11_TRIGGER:
			SWM->PINASSIGN[8] |= (4 << shift);	//	PIO0_04
			break;
	}
}

void SCTimer::enableInt(void){	NVIC->ISER[0] = (1 << SCT0_IRQn);	}

void SCTimer::disableInt(void){	NVIC->ICER[0] = (1 << SCT0_IRQn);	}

void SCTimer::startTimer(void){

}

void SCTimer::stopTimer(void){

}

void SCTimer::resetTimer(void){

}
