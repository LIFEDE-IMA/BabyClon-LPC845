/*
 * inmux.h
 *
 *  Created on: 15 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle inmux peripheral
 */

#ifndef INMUX_H_
#define INMUX_H_

#include "LPC845.h"

class InMux{
	public:
		enum SCT_INMUX_SOURCE_t : uint8_t{
			SCT_PIN0 = 0x0,		//	SCT_PIN0. Assign to pin using the switch matrix
			SCT_PIN1 = 0x1,		//	SCT_PIN1. Assign to pin using the switch matrix
			SCT_PIN2 = 0x2,		//	SCT_PIN2. Assign to pin using the switch matrix
			SCT_PIN3 = 0x3,		//	SCT_PIN3. Assign to pin using the switch matrix
			ADC_THCMP_IRQ = 0x4,
			sctACMP_O = 0x5,
			T0_MAT2 = 0x6,
			GPIOINT_BMATCH = 0x7,
			ARM_TXEV = 0x8,
			DEBUG_HALTED = 0x9
		};

		enum DMA_TRIGGER_INPUT_SOURCE_t : uint8_t{
			ADC_SEQA_IRQ = 0x0,
			ADC_SEQB_IRQ = 0x1,
			SCT_DMA0 = 0x2,
			SCT_DMA1 = 0x3,
			dmaACMP_O = 0x4,
			PININT4 = 0x5,
			PININT5 = 0x6,
			PININT6 = 0x7,
			PININT7 = 0x8,
			T0_DMAREQ_M0 = 0x9,
			T0_DMAREQ_M1 = 0xA,
			DMA_INMUX0 = 0xB,
			DMA_INMUX1 = 0xC
		};

		enum DMA_TRIGGER_INPUT_NUMBER_t : bool{
			TRIGGER_INPUT_11 = 0,
			TRIGGER_INPUT_12 = 1
		};

		enum SCT_INPUT_NUMBER_t : uint8_t{
			INPUT_0 = 0,
			INPUT_1,
			INPUT_2,
			INPUT_3
		};

		InMux(){}	//	Constructor

		static void setDMA_INMUX(DMA_TRIGGER_INPUT_NUMBER_t inmux, uint8_t dmaChannel);					//	Sets [dmaChannel] trigger output as DMA_INMUX[0:1] input
		static void setSCT_INMUX(SCT_INPUT_NUMBER_t sctInputNumber, SCT_INMUX_SOURCE_t sctInputSource);	//	Sets [sctInputSource] as input source for SCT input 0-3
		static void setDMA_TriggerInput(uint8_t dmaChannel, DMA_TRIGGER_INPUT_SOURCE_t dmaTriggerInput);//	Sets [dmaTriggerInput] as trigger input for DMA channel 0-25

		~InMux(){}	//	Destructor
};

#endif /* INMUX_H_ */
