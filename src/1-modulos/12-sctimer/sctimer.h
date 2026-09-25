/*
 * sctimer.h
 *
 *  Created on: 15 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 */

#ifndef SCTIMER_H_
#define SCTIMER_H_

#include "LPC845.h"
#include "inmux.h"

#if defined (__cplusplus)
extern "C" {

void SCT_IRQHandler(void);

}
#endif

class SCTimer{
	public:
		enum sctOpMode_t : uint8_t{
			sctLnH_MODE,		//	Two 16 bits registers
			sctUNIFIED_MODE		//	Only one register (32 bits counter/timer)
		};

		enum sctClkMode_t : uint8_t{
			sctCLKMODE_SYSTEM = 0x0,	//	The system clock clocks the entire SCTimer/PWM module including the counter(s) and counter prescalers
			sctCLKMODE_SAMPLED,			//	System clk clocks SCT/PWM module, but counter and prescalers are only enabled when the designated edge is detected on the input selected by CKSEL. This mode is the high-performance, sampled-clock mode
			sctCLKMODE_INPUT,			//	The input/edge selected by CKSEL clocks SCT/PWM module, after first being sync to the system clk
			sctCLKMODE_ASYNC			//	The input/edge selected by CKSEL clocks directly the entire SCT/PWM module
		};

		enum sctClkSel_t : uint8_t{
			sctCKSEL_RISING_EDGE_INPUT0 = 0x0,
			sctCKSEL_FALLING_EDGE_INPUT0,
			sctCKSEL_RISING_EDGE_INPUT1,
			sctCKSEL_FALLING_EDGE_INPUT1,
			sctCKSEL_RISING_EDGE_INPUT2,
			sctCKSEL_FALLING_EDGE_INPUT2,
			sctCKSEL_RISING_EDGE_INPUT3,
			sctCKSEL_FALLING_EDGE_INPUT3,
			sctCKSEL_RISING_EDGE_INPUT4,
			sctCKSEL_FALLING_EDGE_INPUT4,
		};

		enum outputNumber_t : uint8_t {
			sctOUTPUT_0 = 0,
			sctOUTPUT_1,
			sctOUTPUT_2,
			sctOUTPUT_3,
			sctOUTPUT_4,
			sctOUTPUT_5,
			sctOUTPUT_6
		};

		enum outputType_t : uint8_t{
			sctOUTPUTtype_PIN = 0,
			sctOUTPUTtype_ADC0_TRIGGER,
			sctOUTPUTtype_ADC1_TRIGGER,
			sctOUTPUTtype_ADC2_TRIGGER,
			sctOUTPUTtype_ADC3_TRIGGER,
			sctOUTPUTtype_ADC4_TRIGGER,
			sctOUTPUTtype_ADC5_TRIGGER,
			sctOUTPUTtype_ADC6_TRIGGER,
			sctOUTPUTtype_ADC7_TRIGGER,
			sctOUTPUTtype_ADC8_TRIGGER,
			sctOUTPUTtype_ADC9_TRIGGER,
			sctOUTPUTtype_ADC10_TRIGGER,
			sctOUTPUTtype_ADC11_TRIGGER
		};

		enum counterDir_t : uint8_t{
			sctCountDir_UP = 0,	//	Counts till a limit, then cleared to 0
			sctCountDir_UPnDOWN	//	Counts till a limit, then counts down to a limit or to 0
		};

		enum sctRegisterNumber_t : uint8_t{
			sctREGISTER_0 = 0,
			sctREGISTER_1,
			sctREGISTER_2,
			sctREGISTER_3,
			sctREGISTER_4,
			sctREGISTER_5,
			sctREGISTER_6,
			sctREGISTER_7
		};

		enum sctRegsiterMode_t : uint8_t{
			regMATCH_MODE = 0,
			regCAPTURE_MODE
		};

		enum sctBidirOutputCtrl_t : uint8_t{
			DIR_IRRELEVANT_FOR_OUTPUT = 0,	//	Set and clear do not depend on the direction of any counter
			DIR_TOGGLES_OUTPUT_L,			//	Set and clear are reversed when counter L or the unified counter is counting down
			DIR_TOGGLES_OUTPUT_H			//	Set and clear are reversed when counter H is counting down. Do not use if UNIFY = 1
		};

		enum sctOutputConflictResolution : uint8_t{	//	Specifies what action should be taken if multiple
			DO_NOTHING = 0,							//	events dictate that a given output should be both
			SET_OUTPUT,								//	set and cleared at the same time
			CLEAR_OUTPUT,
			TOGGLE_OUTPUT
		};

		enum sctCounter_t : uint8_t{
			LOW_COUNTER = 0,
			HIGH_COUNTER,
			UNIFIED_COUNTER
		};

		enum sctEvent_t : uint8_t{
			sctEVENT_0 = 0,
			sctEVENT_1,
			sctEVENT_2,
			sctEVENT_3,
			sctEVENT_4,
			sctEVENT_5,
			sctEVENT_6,
			sctEVENT_7
		};

		enum sctEventState_t : uint8_t{
			sctEVENT_STATE_0 = 0,
			sctEVENT_STATE_1,
			sctEVENT_STATE_2,
			sctEVENT_STATE_3,
			sctEVENT_STATE_4,
			sctEVENT_STATE_5,
			sctEVENT_STATE_6,
			sctEVENT_STATE_7
		};

		enum sctEventIOCondition_t : uint8_t{
			LOW_LEVEL = 0,
			RISING_EDGE,
			FALLING_EDGE,
			HIGH_LEVEL
		};

		enum sctEventCombMode_t : uint8_t{
			COMBMODE_OR = 0,	//	The event occurs when either the specified match or I/O condition occurs
			COMBMODE_MATCH,		//	Uses the specified match only
			COMBMODE_IO,		//	Uses the specified I/O condition only
			COMBMODE_AND		//	The event occurs when the specified match and I/O condition occur simultaneously
		};

		enum sctEventMatchCondition : uint8_t{
			EQUAL = 0,				//	A match is only be active during the cycle when the counter is equal to the match value
			EQUAL_OR_GREATER_LESS	//	A match is considered to be active whenever the counter value is GREATER THAN OR EQUAL TO the value specified in the match register when counting up, LESS THEN OR EQUAL TO the match value when counting down
		};

		enum sctEventDirDepending_t : uint8_t{
			DIR_INDEPENDENT = 0,	//	This event is triggered regardless of the count direction
			DIR_UP,					//	This event is triggered only during up-counting when BIDIR = 1
			DIR_DOWN				//	This event is triggered only during down-counting when BIDIR = 1
		};

		enum sctEventStateLoad_t : uint8_t{	//	This controls how the STATEV value modifies the state selected by HEVENT when this event is the highest-numbered event occurring for that state
			STATE_ADD = 0,	//	STATEV value is added into STATE (the carry-out is ignored)
			STATE_LOAD		//	STATEV value is loaded into STATE as the new state
		};

		enum sctTimeBase_t : uint8_t{
			T_MICRO = 0,
			T_MILI,
			T_SEG,
			T_MIN
		};

	private:
		//	GENERAL CONFIG
		sctOpMode_t m_sctOpMode;
		sctClkMode_t m_sctClkMode;
		sctClkSel_t m_sctClkSel;
		bool m_sctReloadL;
		bool m_sctReloadH;
		bool m_sctLimitL;
		bool m_sctLimitH;
		counterDir_t m_sctCounterDirL;
		counterDir_t m_sctCounterDirH;
		uint8_t m_sctPrescalerL;
		uint8_t m_sctPrescalerH;

		//	INPUT CONFIG
		bool m_sctInputTriggerConfigured_flag;
		InMux::SCT_INPUT_NUMBER_t m_sctInputNumber;
		InMux::SCT_INMUX_SOURCE_t m_sctInputSource;
		bool m_sctInputPort;
		uint8_t m_sctInputPin;

		//	OUTPUT CONFIG
		bool m_sctOutputTriggerConfigured_flag;
		outputNumber_t m_sctOutputNumber;
		outputType_t m_sctOutputType;
		bool m_sctOutputPort;
		uint8_t m_sctOutputPin;

		//	ISR
		void (*m_usrHandler)(void);

		void inputConfig(void);		//	Configures SCT Input INMUX and SWM
		void outputConfig(void);	//	Configures SCT Output SWM
		void setOutputADC(outputType_t outputADC);	//	Configures SCT Output PIO when ADC Trigger Output Type is asked

		uint32_t baseToTicks(sctCounter_t counter, uint32_t time, sctTimeBase_t base) const;	//	Converts time into ticks

	public:
		SCTimer(sctOpMode_t opMode = sctOpMode_t::sctUNIFIED_MODE, void (*usrHandler)(void) = nullptr, bool setInputTrigger = false, bool setOutputTrigger = false);	//	Constructor

		void configCLK(sctClkMode_t ClkMode, sctClkSel_t clkSel);		//	Configures CFG: CLKMODE y CKSEL
		void configPrescaler(uint8_t clkDiv_L, uint8_t clkDiv_H = 0);	//	Configures the factor by which the clk is divided (PRE + 1)
		void configReload(sctCounter_t counter, bool reload);			//	Configures CFG: NORELOAD
		void configAutoLimit(sctCounter_t counter, bool limit);			//	Configures CFG: AUTOLIMIT
		void configInput(InMux::SCT_INPUT_NUMBER_t inputNumber, InMux::SCT_INMUX_SOURCE_t inputSource, bool inputPort, uint8_t inputPin);	//	Configures Input INMUX and SWM
		void configOutput(outputNumber_t outputNumber, outputType_t outputType, bool outputPort, uint8_t outputPin);							//	Configures Output SWM
		void configCountDir(sctCounter_t counter, counterDir_t dir);	//	Configures if SCT counts up or up-down
		void configRegisterMode(sctCounter_t counter, sctRegisterNumber_t reg, sctRegsiterMode_t mode);	//	Configures [reg] register as MATCH or CAPTURE
		void configDirOutputCtrl(outputNumber_t outputNumber, sctBidirOutputCtrl_t config);		//	Configures if counter dir has any effect in output set/clear
		void configOutputConflictResolution(outputNumber_t outputNumber, sctOutputConflictResolution resolution);	//	Configures the resolution when output conflict happens
		void configEvent(sctCounter_t counter, sctEvent_t event, sctRegisterNumber_t matchRegSel, sctEventMatchCondition matchCond, sctEventDirDepending_t dirDepending, sctEventStateLoad_t stateLoad = sctEventStateLoad_t::STATE_ADD, sctEventState_t newState = sctEventState_t::sctEVENT_STATE_0) const;	//	Configures Event Trigger Without Any Input/Output
		void configEventInput(sctCounter_t counter, sctEvent_t event, sctRegisterNumber_t matchRegSel, InMux::SCT_INPUT_NUMBER_t inputNumber, sctEventIOCondition_t ioCondition, sctEventCombMode_t combMode, sctEventMatchCondition matchCond, sctEventDirDepending_t dirDepending, sctEventStateLoad_t stateLoad = sctEventStateLoad_t::STATE_ADD, sctEventState_t newState = sctEventState_t::sctEVENT_STATE_0) const;		//	Configures Event Trigger With Input Conditions
		void configEventOutput(sctCounter_t counter, sctEvent_t event, sctRegisterNumber_t matchRegSel, outputNumber_t outputNumber, sctEventIOCondition_t ioCondition, sctEventCombMode_t combMode, sctEventMatchCondition matchCond, sctEventDirDepending_t dirDepending, sctEventStateLoad_t stateLoad = sctEventStateLoad_t::STATE_ADD, sctEventState_t newState = sctEventState_t::sctEVENT_STATE_0) const;				//	Configures Event Trigger With Output Conditions
		void configEventOutputSet(outputNumber_t output, sctEvent_t event) const;		//	Sets [event] event as the setter for [output] output
		void configEventOutputClear(outputNumber_t output, sctEvent_t event) const;		//	Sets [event] event as the clearer for [output] output

		void init(void);			//	Configures SCTimer Peripheral

		void enableNVICint(void) const;		//	Enables NVIC Interrupt
		void disableNVICint(void) const;	//	Disables NVIC Interrupt
		void enableEventInterrupt(sctEvent_t event) const;			//	Enables Event Interrupt 0-7
		void disableEventInterrupt(sctEvent_t event) const;			//	Disables Event Interrupt 0-7
		void enableConflictInterrupt(outputNumber_t output) const;	//	Enables Conflict Interrupt for Output [output]
		void disableConflictInterrupt(outputNumber_t output) const;	//	Disables Conflict Interrupt for Output [output]

		void setReload(sctRegisterNumber_t reg, uint32_t reloadValue) const;							//	Sets reload value for reg 0-7 in unify mode
		void setReload(sctCounter_t counter, sctRegisterNumber_t reg, uint16_t reloadValue) const;		//	Sets reload value for reg 0-7 in low counter or high counter
		void setCaptureTrigger(sctCounter_t counter, sctRegisterNumber_t reg, sctEvent_t event) const;	//	Sets [event] event as the trigger for counter value to be loaded in capture register [reg]
		void setLimit(sctCounter_t counter, sctEvent_t event) const;		//	Sets [event] Event as Limit for the Counter
		void clearLimit(sctCounter_t counter, sctEvent_t event) const;		//	Clears [event] Event as Limit for the Counter

		void enableStateEvent(sctEvent_t event, sctEventState_t state) const;	//	Enables event [event] in [state] state
		void disableStateEvent(sctEvent_t event, sctEventState_t state) const;	//	Disables event [event] in [state] state
		void setState(sctCounter_t counter, sctEventState_t state) const;		//	Sets the initial/current state
		sctEventState_t getState(sctCounter_t counter) const;					//	Returns current state

		uint32_t getCount(sctCounter_t counter) const;	//	Returns Count for Unify, Low or High Counter
		uint32_t getCapture(sctCounter_t counter, sctRegisterNumber_t reg) const;	//	Returns Counter Value when Events Selected by Capture Ctrl Reg Occur

		void setTimer(sctCounter_t counter, sctRegisterNumber_t reg, uint32_t time, sctTimeBase_t base = sctTimeBase_t::T_SEG) const;	//	Configures MATCH[reg] to count till [time]
		void startCounter(sctCounter_t counter) const;		//	Starts SCTimer Count (unify, low or high counter)
		void stopCounter(sctCounter_t counter) const;		//	Stops SCTimer Count (unify, low or high counter)
		void haltCounter(sctCounter_t counter) const;		//	Halts SCTimer (unify, low or high counter)

		uint8_t getIntFlags(void) const;				//	Returns Active Interrupt Flags
		void clearIntFlags(uint8_t flags) const;		//	Clears Active Interrupt Flags
		void clearEventIntFlag(sctEvent_t event) const;	//	Clears Event Interrupt Flag
		uint32_t getConflictFlags(void) const;			//	Returns Active Conflict Flags
		void clearConflictFlags(uint32_t flags) const;	//	Clears Active Conflict Flags

		void setOutput(outputNumber_t output) const;					//	Forces "1" In [output] Output (SCTimer HAS to be Halted)
		void setStop(sctCounter_t counter, sctEvent_t event) const;		//	[event] Event Stops [counter] Count
		void clrOutput(outputNumber_t output) const;					//	Forces "0" In [output] Output (SCTimer HAS to be Halted)
		void clrStop(sctCounter_t counter, sctEvent_t event) const;		//	[event] Event NO More Stops [counter] Count

		void isrHandler(void);	//	Member ISR Handler (called whenever an event triggers an interrupt)

		friend void SCT_IRQHandler(void);	//	ISR Handler for SCTimer

		~SCTimer(){}	//	Destructor
};

#endif /* SCTIMER_H_ */
