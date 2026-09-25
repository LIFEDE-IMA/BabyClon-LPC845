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

SCTimer *SCTinstance = nullptr;

//	NVIC (Cap. 7), SYSCON (Cap. 8), SWM (Cap. 10), INPUT MUX (Cap. 14), SCTIMER (Cap. 21)
SCTimer::SCTimer(sctOpMode_t opMode, void (*usrHandler)(void), bool setInputTrigger, bool setOutputTrigger){
	SCTinstance = this;

	m_usrHandler = usrHandler;

	m_sctOpMode = opMode;
	m_sctClkMode = sctClkMode_t::sctCLKMODE_SYSTEM;		//	Default

	m_sctInputTriggerConfigured_flag = setInputTrigger;
	m_sctOutputTriggerConfigured_flag = setOutputTrigger;

	m_sctClkSel = sctClkSel_t::sctCKSEL_RISING_EDGE_INPUT0;	//	Default
	m_sctReloadL = false;	//	Default
	m_sctReloadH = true;	//	Default
	m_sctLimitL = false;	//	Default
	m_sctLimitH = false;	//	Default

	m_sctCounterDirL = counterDir_t::sctCountDir_UP;	//	Default
	m_sctCounterDirH = counterDir_t::sctCountDir_UP;	//	Default

	m_sctPrescalerL = 0;	//	Default
	m_sctPrescalerH = 0;	//	Default

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

	SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 8));	//	Enable Clk for SWM and SCT

	SYSCON->SCTCLKSEL |= 0x1;	//	SCT Clk: Main Clk
	SYSCON->SCTCLKDIV |= 0X1;	//	SCT ClkDiv: 1

	//	RESET SCT
	SYSCON->PRESETCTRL0 &= ~(1 << 8);	//	Assert Reset
	SYSCON->PRESETCTRL0 |= (1 << 8);	//	Clear Reset
}

void SCTimer::configCLK(sctClkMode_t ClkMode, sctClkSel_t clkSel){
	m_sctClkMode = ClkMode;
	m_sctClkSel = clkSel;
}

void SCTimer::configPrescaler(uint8_t clkDiv_L, uint8_t clkDiv_H){
	m_sctPrescalerL = clkDiv_L;

	if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)	m_sctPrescalerH = clkDiv_H;
}

void SCTimer::configReload(sctCounter_t counter, bool reload){
	if(counter != sctCounter_t::HIGH_COUNTER)
		m_sctReloadL = reload;

	if((counter == sctCounter_t::HIGH_COUNTER) && (m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE))
		m_sctReloadH = reload;
}

void SCTimer::configAutoLimit(sctCounter_t counter, bool limit){
	if(counter != sctCounter_t::HIGH_COUNTER)
		m_sctLimitL = limit;

	if((counter == sctCounter_t::HIGH_COUNTER) && (m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE))
		m_sctLimitH = limit;
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

void SCTimer::configCountDir(sctCounter_t counter, counterDir_t dir){
	if(counter != sctCounter_t::HIGH_COUNTER)
		m_sctCounterDirL = dir;
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		m_sctCounterDirH = dir;
}

void SCTimer::configRegisterMode(sctCounter_t counter, sctRegisterNumber_t reg, sctRegsiterMode_t mode){
	if((counter != sctCounter_t::HIGH_COUNTER) || (m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE)){
		if(mode){
			SCT->REGMODE.REGMODE_L |= (1 << reg);	//	Set [reg] bit in capture for low counter
		}else{
			SCT->REGMODE.REGMODE_L &= ~(1 << reg);	//	Set [reg] bit in match [mode] for low counter
		}
	}
	if((counter == sctCounter_t::HIGH_COUNTER) && (m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)){
		if(mode){
			SCT->REGMODE.REGMODE_H |= (1 << reg);	//	Set [reg] bit in capture for high counter
		}else{
			SCT->REGMODE.REGMODE_H &= ~(1 << reg);	//	Set [reg] bit in match for high counter
		}
	}
}

void SCTimer::configDirOutputCtrl(outputNumber_t outputNumber, sctBidirOutputCtrl_t config){
	uint8_t shift = (outputNumber * 2);

	SCT->OUTPUTDIRCTRL &= ~(0x3 << shift);	//	Clears [outputNumber] OUTPUTDIRCTRL

	if(config != sctBidirOutputCtrl_t::DIR_TOGGLES_OUTPUT_H){
		SCT->OUTPUTDIRCTRL |= (config << shift);
	}else{
		if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
			SCT->OUTPUTDIRCTRL |= (config << shift);
		}
	}
}

void SCTimer::configOutputConflictResolution(outputNumber_t outputNumber, sctOutputConflictResolution resolution){
	uint8_t shift = (outputNumber * 2);
	SCT->RES &= ~(0x3 << shift);
	SCT->RES |= (resolution << shift);
}

void SCTimer::configEvent(sctCounter_t counter, sctEvent_t event, sctRegisterNumber_t matchRegSel, sctEventMatchCondition matchCond, sctEventDirDepending_t dirDepending, sctEventStateLoad_t stateLoad, sctEventState_t newState) const{
	bool hevent = (counter == sctCounter_t::HIGH_COUNTER);

	SCT->EV[event].CTRL = ((matchRegSel << 0)	|	//	Selects the Match register associated with this event
						   (hevent << 4)		|	//	Select L/H counter
						   (1 << 12)			|	//	COMBMODE = MATCH. Uses the specified match only
						   (stateLoad << 14)	|	//	STATELD: This bit controls how the STATEV value modifies the state selected by HEVENT when this event is the highest-numbered event occurring for that state
						   (newState << 15)		|	//	STATEV: This value is loaded into or added to the state selected by HEVENT, depending on STATELD
						   (matchCond << 20)	|	//	1: match is considered to be active whenever the counter value >= the value specified in the match register when counting up, <= the match value when counting down. 0: a match is only be active during the cycle when the counter = the match value
						   (dirDepending << 21));	//	Determines if event is triggered depending on counter direction
}

void SCTimer::configEventInput(sctCounter_t counter, sctEvent_t event, sctRegisterNumber_t matchRegSel, InMux::SCT_INPUT_NUMBER_t inputNumber, sctEventIOCondition_t ioCondition, sctEventCombMode_t combMode, sctEventMatchCondition matchCond, sctEventDirDepending_t dirDepending, sctEventStateLoad_t stateLoad, sctEventState_t newState) const{
	bool hevent = 0;

	if(counter == sctCounter_t::HIGH_COUNTER)
		hevent = 1;

	SCT->EV[event].CTRL = ((matchRegSel << 0)	|	//	Selects the Match register associated with this event
						   (hevent << 4)		|	//	Select L/H counter
						   (0 << 5)				|	//	Input Triggered
						   (inputNumber << 6)	|	//	Select Input
						   (ioCondition << 10)	|	//	Select Input Condition for the Trigger
						   (combMode << 12)		|	//	Select how the specified match and I/O condition are used and combined
						   (stateLoad << 14)	|	//	STATELD: This bit controls how the STATEV value modifies the state selected by HEVENT when this event is the highest-numbered event occurring for that state
						   (newState << 15)		|	//	STATEV: This value is loaded into or added to the state selected by HEVENT, depending on STATELD
						   (matchCond << 20)	|	//	1: match is considered to be active whenever the counter value >= the value specified in the match register when counting up, <= the match value when counting down. 0: a match is only be active during the cycle when the counter = the match value
						   (dirDepending << 21));	//	Determines if event is triggered depending on counter direction
}

void SCTimer::configEventOutput(sctCounter_t counter, sctEvent_t event, sctRegisterNumber_t matchRegSel, outputNumber_t outputNumber, sctEventIOCondition_t ioCondition, sctEventCombMode_t combMode, sctEventMatchCondition matchCond, sctEventDirDepending_t dirDepending, sctEventStateLoad_t stateLoad, sctEventState_t newState) const{
	bool hevent = 0;

	if(counter == sctCounter_t::HIGH_COUNTER)
		hevent = 1;

	SCT->EV[event].CTRL = ((matchRegSel << 0)	|	//	Selects the Match register associated with this event
						   (hevent << 4)		|	//	Select L/H counter
						   (1 << 5)				|	//	Output Triggered
						   (outputNumber << 6)	|	//	Select Input
						   (ioCondition << 10)	|	//	Select Input Condition for the Trigger
						   (combMode << 12)		|	//	Select how the specified match and I/O condition are used and combined
						   (stateLoad << 14)	|	//	STATELD: This bit controls how the STATEV value modifies the state selected by HEVENT when this event is the highest-numbered event occurring for that state
						   (newState << 15)		|	//	STATEV: This value is loaded into or added to the state selected by HEVENT, depending on STATELD
						   (matchCond << 20)	|	//	1: match is considered to be active whenever the counter value >= the value specified in the match register when counting up, <= the match value when counting down. 0: a match is only be active during the cycle when the counter = the match value
						   (dirDepending << 21));	//	Determines if event is triggered depending on counter direction
}

void SCTimer::configEventOutputSet(outputNumber_t output, sctEvent_t event) const{
	SCT->OUT[output].SET |= (1 << event);	//	Selects event to set output
}

void SCTimer::configEventOutputClear(outputNumber_t output, sctEvent_t event) const{
	SCT->OUT[output].CLR |= (1 << event);	//	Selects event to clear output
}

void SCTimer::init(void){
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

	//	SCT CTRL
	SCT->CTRL.CTRL_L |= ((1 << 2)				 |		//	HALT_L = 1
						 (1 << 3)				 |		//	CLRCTR_L = 1
						 (m_sctCounterDirL << 4) |		//	Sets counter dir (up or up-down)
						 (m_sctPrescalerL << 5));		//	Sets clk divider (clkDiv_L + 1)

	if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
		SCT->CTRL.CTRL_H |= ((1 << 2)				 |		//	HALT_H = 1
				 	 	 	 (1 << 3)				 |		//	CLRCTR_H = 1
							 (m_sctCounterDirH << 4) |		//	Sets counter dir (up or up-down)
							 (m_sctPrescalerH << 5));		//	Sets clk divider (clkDiv_H + 1)
	}
}

void SCTimer::inputConfig(void){
	InMux::setSCT_INMUX(m_sctInputNumber, m_sctInputSource);
	if(m_sctInputSource <= InMux::SCT_INMUX_SOURCE_t::SCT_PIN3){	//	Input Source is a GPIO
		uint8_t inputGPIO = (m_sctInputPin + (m_sctInputPort * 32));
		if(m_sctInputSource == InMux::SCT_INMUX_SOURCE_t::SCT_PIN0){
			SWM->PINASSIGN[6] &= ~(0xFF << 24);
			SWM->PINASSIGN[6] |= (inputGPIO << 24);		//	SCT Input PIN 0
		}else{
			uint8_t shift = ((m_sctInputSource - 1) * 8);
			SWM->PINASSIGN[7] &= ~(0xFF << shift);
			SWM->PINASSIGN[7] |= (inputGPIO << shift);	//	SCT Input PIN 1-3
		}
	}
}

void SCTimer::outputConfig(void){
	uint8_t outputGPIO = (m_sctOutputPin + (m_sctOutputPort * 32));

	if(m_sctOutputNumber == outputNumber_t::sctOUTPUT_0){
		SWM->PINASSIGN[7] &= ~(0xFF << 24);
		SWM->PINASSIGN[7] |= (outputGPIO << 24);	//	SCT Output 0
	}else if(m_sctOutputNumber <= outputNumber_t::sctOUTPUT_4){
		if(m_sctOutputType == outputType_t::sctOUTPUTtype_PIN){	//	Output is selected as GPIO
			uint8_t shift = ((m_sctOutputNumber - 1) * 8);
			SWM->PINASSIGN[8] &= ~(0xFF << shift);
			SWM->PINASSIGN[8] |= (outputGPIO << shift);	//	SCT Output 1-4
		}else if(m_sctOutputNumber >= outputNumber_t::sctOUTPUT_3){	//	Only SCT Output 3-4 can be set as ADC Input Trigger
			SCTimer::setOutputADC(m_sctOutputType);	//	SCT Output ADC0-11
		}
	}else{
		uint8_t shift = ((m_sctOutputNumber - 5) * 8);
		SWM->PINASSIGN[9] &= ~(0xFF << shift);
		SWM->PINASSIGN[9] |= (outputGPIO << shift);	//	SCT Output 5-9
	}
}

void SCTimer::setOutputADC(outputType_t outputADC){
	uint8_t shift = ((m_sctOutputNumber - 1) * 8);
	SWM->PINASSIGN[8] &= ~(0xFF << shift);

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

void SCTimer::enableNVICint(void) const{
	NVIC->ICPR[0] = (1 << SCT0_IRQn);
	NVIC->ISER[0] = (1 << SCT0_IRQn);
}

void SCTimer::disableNVICint(void) const{ NVIC->ICER[0] = (1 << SCT0_IRQn); }

void SCTimer::enableEventInterrupt(sctEvent_t event) const{ SCT->EVEN |= (1 << event); }

void SCTimer::disableEventInterrupt(sctEvent_t event) const{ SCT->EVEN &= ~(1 << event); }

void SCTimer::enableConflictInterrupt(outputNumber_t output) const{ SCT->CONEN |= (1 << output); }

void SCTimer::disableConflictInterrupt(outputNumber_t output) const{ SCT->CONEN &= ~(1 << output); }

void SCTimer::setReload(sctRegisterNumber_t reg, uint32_t reloadValue) const{
	bool regmode = (((SCT->REGMODE.REGMODE_L) >> reg) & 0x1);
	if(regmode == sctRegsiterMode_t::regMATCH_MODE){
		if(m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE){
			SCT->MATCHREL_CAPCTRL[reg].MATCHREL_L = (uint16_t)reloadValue;
			SCT->MATCHREL_CAPCTRL[reg].MATCHREL_H = (uint16_t)(reloadValue >> 16);
		}
	}
}

void SCTimer::setReload(sctCounter_t counter, sctRegisterNumber_t reg, uint16_t reloadValue) const{
	bool regmode = (((SCT->REGMODE.REGMODE_L) >> reg) & 0x1);

	if(counter == sctCounter_t::HIGH_COUNTER)
		regmode = (((SCT->REGMODE.REGMODE_H) >> reg) & 0x1);

	if(regmode == sctRegsiterMode_t::regMATCH_MODE){
		if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
			if(counter == sctCounter_t::LOW_COUNTER){
				SCT->MATCHREL_CAPCTRL[reg].MATCHREL_L = reloadValue;
			}else if(counter == sctCounter_t::HIGH_COUNTER){
				SCT->MATCHREL_CAPCTRL[reg].MATCHREL_H = reloadValue;
			}
		}
	}
}

void SCTimer::setCaptureTrigger(sctCounter_t counter, sctRegisterNumber_t reg, sctEvent_t event) const{
	bool regmode = (((SCT->REGMODE.REGMODE_L) >> reg) & 0x1);

	if(counter == sctCounter_t::HIGH_COUNTER)
		regmode = (((SCT->REGMODE.REGMODE_H) >> reg) & 0x1);

	if(regmode == sctRegsiterMode_t::regCAPTURE_MODE){
		if(counter != sctCounter_t::HIGH_COUNTER){
			SCT->MATCHREL_CAPCTRL[reg].CAPCTRL_L |= (1 << event);
		}else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
			SCT->MATCHREL_CAPCTRL[reg].CAPCTRL_H |= (1 << event);
		}
	}
}

void SCTimer::setLimit(sctCounter_t counter, sctEvent_t event) const{
	if(counter != sctCounter_t::HIGH_COUNTER)
		SCT->LIMIT.LIMIT_L |= (1 << event);
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		SCT->LIMIT.LIMIT_H |= (1 << event);
}

void SCTimer::clearLimit(sctCounter_t counter, sctEvent_t event) const{
	if(counter != sctCounter_t::HIGH_COUNTER)
		SCT->LIMIT.LIMIT_L &= ~(1 << event);
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		SCT->LIMIT.LIMIT_H &= ~(1 << event);
}

void SCTimer::enableStateEvent(sctEvent_t event, sctEventState_t state) const{
	SCT->EV[event].STATE |= (1 << state);
}

void SCTimer::disableStateEvent(sctEvent_t event, sctEventState_t state) const{
	SCT->EV[event].STATE &= ~(1 << state);
}

void SCTimer::setState(sctCounter_t counter, sctEventState_t state) const{
	if(counter != sctCounter_t::HIGH_COUNTER)
		SCT->STATE.STATE_L = state;
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		SCT->STATE.STATE_H = state;
}

SCTimer::sctEventState_t SCTimer::getState(sctCounter_t counter) const{
	if(counter != sctCounter_t::HIGH_COUNTER)
		return (sctEventState_t)((SCT->STATE.STATE_L) & 0x7);
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		return (sctEventState_t)((SCT->STATE.STATE_H) & 0x7);
	return sctEventState_t::sctEVENT_STATE_0;
}


uint32_t SCTimer::getCount(sctCounter_t counter) const{
	uint32_t count;

	if(m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE){
		count = SCT->COUNT.COUNTU;
	}else{
		if(counter != sctCounter_t::HIGH_COUNTER){
			count = SCT->COUNT.COUNT_L;
		}else{
			count = SCT->COUNT.COUNT_H;
		}
	}
	return count;
}

uint32_t SCTimer::getCapture(sctCounter_t counter, sctRegisterNumber_t reg) const{
	uint32_t count;

	if(m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE){
		count = (uint32_t)SCT->MATCH_CAP[reg].CAP_L;
		count |= ((SCT->MATCH_CAP[reg].CAP_H) << 16);
	}else{
		if(counter != sctCounter_t::HIGH_COUNTER){
			count = SCT->MATCH_CAP[reg].CAP_L;
		}else{
			count = SCT->MATCH_CAP[reg].CAP_H;
		}
	}
	return count;
}

uint32_t SCTimer::baseToTicks(sctCounter_t counter, uint32_t time, sctTimeBase_t base) const{
	uint32_t ticks = 0;
	uint32_t freq = 0;
	uint64_t product;

	if(counter != sctCounter_t::HIGH_COUNTER){
		freq = (FREQ_CLOCK / (m_sctPrescalerL + 1));
	}else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
		freq = (FREQ_CLOCK / (m_sctPrescalerH + 1));
	}

	product = ((uint64_t)time * freq);	//	uint32_t can overflow

	switch(base){
		case sctTimeBase_t::T_MICRO:
			ticks = (uint32_t)((product / 1000000) - 1);
			break;

		case sctTimeBase_t::T_MILI:
			ticks = (uint32_t)((product / 1000) - 1);
			break;

		case sctTimeBase_t::T_SEG:
			ticks = (uint32_t)(product - 1);
			break;

		case sctTimeBase_t::T_MIN:
			ticks = (uint32_t)((product * 60) - 1);
			break;

		default:
			break;
	}
	return ticks;
}

void SCTimer::setTimer(sctCounter_t counter, sctRegisterNumber_t reg, uint32_t time, sctTimeBase_t base) const{
	uint32_t ticks = SCTimer::baseToTicks(counter, time, base);

	SCTimer::haltCounter(counter);

	if(m_sctOpMode == sctOpMode_t::sctUNIFIED_MODE){
		SCT->MATCH_CAP[reg].MATCH_L = (uint16_t)ticks;
		SCT->MATCH_CAP[reg].MATCH_H = (uint16_t)(ticks >> 16);
	}else{
		if(counter != sctCounter_t::HIGH_COUNTER){
			SCT->MATCH_CAP[reg].MATCH_L = (uint16_t)ticks;
		}else{
			SCT->MATCH_CAP[reg].MATCH_H = (uint16_t)ticks;
		}
	}

}

void SCTimer::startCounter(sctCounter_t counter) const{
	if(counter != sctCounter_t::HIGH_COUNTER){
		SCT->CTRL.CTRL_L &= ~((1 << 1) | (1 << 2));	//	Clear STOP_L y HALT_L
	}else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
		SCT->CTRL.CTRL_H &= ~((1 << 1) | (1 << 2));	//	Clear STOP_H y HALT_H
	}
}

void SCTimer::stopCounter(sctCounter_t counter) const{
	if(counter != sctCounter_t::HIGH_COUNTER){
		SCT->CTRL.CTRL_L &= ~(1 << 2);	//	Clear HALT_L
		SCT->CTRL.CTRL_L |= (1 << 1);	//	Set STOP_L
	}else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
		SCT->CTRL.CTRL_H &= ~(1 << 2);	//	Clear HALT_H
		SCT->CTRL.CTRL_H |= (1 << 1);	//	Set STOP_H
	}
}

void SCTimer::haltCounter(sctCounter_t counter) const{
	if(counter != sctCounter_t::HIGH_COUNTER){
		SCT->CTRL.CTRL_L |= (1 << 2);	//	Set HALT_L
	}else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE){
		SCT->CTRL.CTRL_H |= (1 << 2);	//	Set HALT_H
	}
}

uint8_t SCTimer::getIntFlags(void) const{ return (SCT->EVFLAG & 0xFF); }

void SCTimer::clearIntFlags(uint8_t flags) const{ SCT->EVFLAG = (flags & 0xFF); }

void SCTimer::clearEventIntFlag(sctEvent_t event) const{ SCT->EVFLAG = (1 << event); }

uint32_t SCTimer::getConflictFlags(void) const{
	uint32_t mask = (0x3F | (0x3 << 30));
	return (SCT->CONFLAG & mask);
}

void SCTimer::clearConflictFlags(uint32_t flags) const{
	uint32_t mask = (0x3F | (0x3 << 30));
	SCT->CONFLAG = (flags & mask);
}

void SCTimer::setOutput(outputNumber_t output) const{
	SCT->OUTPUT |= (1 << output);	//	(SCTimer HAS to be Halted)
}

void SCTimer::setStop(sctCounter_t counter, sctEvent_t event) const{
	if(counter != sctCounter_t::HIGH_COUNTER)
		SCT->STOP.STOP_L |= (1 << event);
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		SCT->STOP.STOP_H |= (1 << event);
}

void SCTimer::clrOutput(outputNumber_t output) const{
	SCT->OUTPUT &= ~(1 << output);	//	(SCTimer HAS to be Halted)
}

void SCTimer::clrStop(sctCounter_t counter, sctEvent_t event) const{
	if(counter != sctCounter_t::HIGH_COUNTER)
		SCT->STOP.STOP_L &= ~(1 << event);
	else if(m_sctOpMode != sctOpMode_t::sctUNIFIED_MODE)
		SCT->STOP.STOP_H &= ~(1 << event);
}

void SCTimer::isrHandler(void){
	if(m_usrHandler)
		m_usrHandler();
}

void SCT_IRQHandler(void){
	if(SCTinstance)
		SCTinstance->isrHandler();
}
