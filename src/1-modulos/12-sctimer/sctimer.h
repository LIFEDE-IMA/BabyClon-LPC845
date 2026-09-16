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

	private:
		//	GENERAL CONFIG
		sctOpMode_t m_sctOpMode;
		sctClkMode_t m_sctClkMode;
		sctClkSel_t m_sctClkSel;
		bool m_sctReloadL;
		bool m_sctReloadH;
		bool m_sctLimitL;
		bool m_sctLimitH;

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

		void inputConfig(void);		//	Configures SCT Input INMUX and SWM
		void outputConfig(void);	//	Configures SCT Output SWM
		void setOutputADC(outputType_t outputADC);	//	Configures SCT Output PIO when ADC Trigger Output Type is asked

		void resetTimer(void);	//	Restarts SCTimer Count

	public:
		SCTimer(sctOpMode_t opMode = sctOpMode_t::sctUNIFIED_MODE, bool setInputTrigger = false, bool setOutputTrigger = false);	//	Constructor

		void configCLK(sctClkMode_t ClkMode, sctClkSel_t clkSel);	//	Configures CFG: CLKMODE y CKSEL
		void configReload(bool reloadL, bool reloadH);				//	Configures CFG: NORELOAD
		void configAutoLimit(bool limitL, bool limitH);				//	Configures CFG: AUTOLIMIT
		void configInput(InMux::SCT_INPUT_NUMBER_t inputNumber, InMux::SCT_INMUX_SOURCE_t inputSource, bool inputPort, uint8_t inputPin);	//	Configures Input INMUX and SWM
		void configOutput(outputNumber_t outputNumber, outputType_t outputType, bool outputPort, uint8_t outputPin);							//	Configures Output SWM

		void init(void);			//	Configures SCTimer Peripheral

		void enableInt(void);	//	Enables NVIC Interrupt
		void disableInt(void);	//	Disables NVIC Interrupt

		void startTimer(void);	//	Starts SCTimer Count
		void stopTimer(void);	//	Stops SCTimer Count

		~SCTimer(){}	//	Destructor
};

#endif /* SCTIMER_H_ */
