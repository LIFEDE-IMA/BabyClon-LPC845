/*
 * ctimer.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 */

#include "ctimer.h"

Ctimer *ctimer = nullptr;

Ctimer::Ctimer(initType_t clkType){
	ctimer = this;
	m_startTime = m_ms = m_us = 0;
	m_clkType = clkType;

	SYSCON->SYSAHBCLKCTRL0 |= (1 << 25);	//	Enable CTIMER0 clock

	//	Reset CTIMER0 peripheral
	SYSCON->PRESETCTRL0 &= ~(1 << 25);	//	RESET CTIMER0
	SYSCON->PRESETCTRL0 |= (1 << 25);	//	RELEASE RESET CTIMER0

	CTIMER0->CTCR &= ~((1 << 0) | (1 << 1));	//	Timer mode

	CTIMER0->PR = ((FREQ_CLOCK / 1000000) - 1);	//	Main_clk / (PR + 1) = 30MHz / 30 = 1Mhz

	switch(m_clkType){
		case initType_t::NORMAL_CLK:
			Ctimer::init();
			break;

		case initType_t::FREE_CLK:
			Ctimer::initFreeClk();
			break;

		case initType_t::CAPTURE_CLK:
			Ctimer::initCaptureClk();
			break;

		case initType_t::HYBRID_CLK:
			Ctimer::initHybrid();
			break;

		default:
			//	ERROR
			break;
	}

	NVIC->ISER[0] = (1 << CTIMER0_IRQn);	//	Enable NVIC interrupt

	CTIMER0->TCR |= (1 << 0);	//	Start Timer
}

void Ctimer::init(){		//	NVIC (Cap. 7), SYSCON (Cap. 8), CTIMER (Cap. 20)
	CTIMER0->MR[0] = 1;	//	Match per 1 us (MR / (Main_clk / (PR + 1)))

	CTIMER0->MCR = ((1 << 0) | (1 << 1));	//	Interrupt & reset TC if MR0 == TC
}

void Ctimer::initFreeClk(){		// NVIC (Cap. 7), SYSCON (Cap. 8), CTIMER (Cap. 20)
	CTIMER0->MR[0] = 0;	//	NO match register

	CTIMER0->MCR = (1 << 0);	//	Interrupt if TC == MR0
}

void Ctimer::initCaptureClk(){		//	NVIC (Cap. 7), SYSCON (Cap. 8), CTIMER (Cap. 20)
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 7);	//	Enables clk for SWM

	SWM->PINASSIGN[14] &= ~(0xFF << 8);
	SWM->PINASSIGN[14] |= (0x6 << 8);	//	T0_CAP0 function assigned to PIO0_06

	CTIMER0->CCR = ((1 << 0) | (1 << 1) | (1 << 2));	//	Capture Rising + Falling Edge => CR0 = TC
														//	Interrupt enabled
	CTIMER0->MR[0] = 0;	//	NO match register (free clock)

	CTIMER0->MCR = 0;	//	NO Interrupt if MR0 == TC
}

void Ctimer::initHybrid(){		// NVIC (Cap. 7), SYSCON (Cap. 8), CTIMER (Cap. 20)
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 7);	//	Enables clk for SWM

	//	SWM->PINASSIGN[14] &= ~(0xFF << 8);
	//	SWM->PINASSIGN[14] |= (0x6 << 8);	//	T0_CAP0 function assigned to PIO0_06
	//	If commented, pin starts as GPIO

	CTIMER0->CCR = ((1 << 0) | (1 << 2));	//	Capture Rising Edge => CR0 = TC
											//	Interrupt enabled
	CTIMER0->MR[0] = 0;	//	NO match register (free clock)

	CTIMER0->MCR = (1 << 0);	//	Interrupt if TC == MR0
}

void Ctimer::set_usTimer(uint32_t us){
	m_us = CTIMER0->TC;
	CTIMER0->MR[0] = m_us + us;
	CTIMER0->IR = (1 << 0);		//	Reset Match 0 Interrupt
	CTIMER0->MCR = (1 << 0);	//	Interrupt if TC == MR0
}

void Ctimer::stop_usTimer(){
	CTIMER0->MCR = 0;	//	Stop Interrupt if TC == MR0
}

void Ctimer::isrHandler(){
	if(CTIMER0->IR & (1 << 0)){		//	Match Interrupt
		CTIMER0->IR |= (1 << 0);	//	Reset Match 0 Interrupt
	}
}

void Ctimer::freeClk_isrHandler(){
	if(CTIMER0->IR & (1 << 0)){		//	Match Interrupt
		CTIMER0->IR |= (1 << 0);	//	Reset Match 0 Interrupt
	}
}

void Ctimer::captClk_isrHandler(){
	if(CTIMER0->IR & (1 << 4)){		//	Capture Interrupt
		CTIMER0->IR |= (1 << 4);		//	Reset Capture 0 Interrupt
	}
}

void Ctimer::hybClk_isrHandler(){
	if(CTIMER0->IR & (1 << 0)){		//	Match Interrupt
		CTIMER0->IR |= (1 << 0);	//	Reset Match 0 Interrupt
	}
	if(CTIMER0->IR & (1 << 4)){		//	Capture Interrupt
		CTIMER0->IR |= (1 << 4);		//	Reset Capture 0 Interrupt
	}
}

void CTIMER0_IRQHandler(void){
	if(ctimer){
		switch(ctimer->m_clkType){
			case Ctimer::NORMAL_CLK:
				ctimer->isrHandler();
				break;

			case Ctimer::FREE_CLK:
				ctimer->freeClk_isrHandler();
				break;

			case Ctimer::CAPTURE_CLK:
				ctimer->captClk_isrHandler();
				break;

			case Ctimer::HYBRID_CLK:
				ctimer->hybClk_isrHandler();
				break;

			default:
				//	ERROR
				break;
		}
	}
}

Ctimer::~Ctimer(){}
