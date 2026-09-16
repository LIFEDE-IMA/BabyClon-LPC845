/*
 * inmux.cpp
 *
 *  Created on: 15 sep. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle inmux peripheral
 */

#include "inmux.h"

void InMux::setDMA_INMUX(DMA_TRIGGER_INPUT_NUMBER_t inmux, uint8_t dmaChannel) const{
	if(dmaChannel <= 25)
		INMUX->DMA_INMUX[inmux] |= dmaChannel;
}

void InMux::setSCT_INMUX(SCT_INPUT_NUMBER_t sctInputNumber, SCT_INMUX_SOURCE_t sctInputSource) const{
	INMUX->SCT_INMUX[sctInputNumber] |= sctInputSource;
}

void InMux::setDMA_TriggerInput(uint8_t dmaChannel, DMA_TRIGGER_INPUT_SOURCE_t dmaTriggerInput) const{
	if(dmaChannel <= 25)
			INMUX->DMA_ITRIG[dmaChannel] |= dmaTriggerInput;
}


