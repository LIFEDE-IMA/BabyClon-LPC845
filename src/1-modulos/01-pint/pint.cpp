/*
 * pint.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "pint.h"

Pint *pinInt_instance[Pint::TOTAL_PINT_SOURCES] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
uint8_t Pint::m_usedSources = 0;

Pint::Pint(bool port, uint8_t pin, triggerType_t trigger, void (*handler)(void), pintSource_t pintSource) :
m_port(port), m_pin(pin), m_trigger(trigger), m_handler(handler){	//	If handler == nullptr -> DMA

	m_pintSource = pintSource_t::PINTSEL0;

	if(pintSource == pintSource_t::ANY){
		for(uint8_t idx = 0; idx < Pint::TOTAL_PINT_SOURCES; idx++){
			if(pinInt_instance[idx] == nullptr){
				m_pintSource = (pintSource_t)idx;
				break;	//	Out of For
			}
		}
	}else{
		m_pintSource = pintSource;
	}

	pinInt_instance[m_pintSource] = this;
	m_usedSources++;

//	NVIC->ICER[0] = (1 << (PIN_INT0_IRQn + m_pintSource));	//	Disables NVIC interrupt for PININTn

	SYSCON->SYSAHBCLKCTRL0 |= (1 << 28);	//	Enables PININT clk

	SYSCON->PINTSEL[m_pintSource] &= ~(0x3F << 0);		//	Clears PININTn source

	SYSCON->PINTSEL[m_pintSource] |= ((m_pin + (m_port * 32)) << 0);	//	Assigns m_pin as PININTn source

	Pint::configIntExt();

	NVIC->ISER[0] = (1 << (PIN_INT0_IRQn + m_pintSource));	//	Enables NVIC interrupt for PININTn
}

void Pint::configIntExt(){
	switch(m_trigger){
		case triggerType_t::T_FALLINGEDGE:
			PINT->ISEL &= ~(1 << m_pintSource);			//	0: Edge Sensitive, 1: Level sensitive
			PINT->IENR &= ~(1 << m_pintSource);			//	0: Disables Rising Edge / Level Interrupt, 1: Enables
			PINT->IENF |= (1 << m_pintSource);			//	0: Disables Falling Edge Interrupt / Sets Level	Interrupt LOW, 1: Enables Falling Edge Interrupt / Sets Level Interrupt HIGH
			break;

		case triggerType_t::T_RISINGEDGE:
			PINT->ISEL &= ~(1 << m_pintSource);			//	0: Edge Sensitive, 1: Level sensitive
			PINT->IENR |= (1 << m_pintSource);			//	0: Disables Rising Edge / Level Interrupt, 1: Enables
			PINT->IENF &= ~(1 << m_pintSource);			//	0: Disables Falling Edge Interrupt / Sets Level	Interrupt LOW, 1: Enables Falling Edge Interrupt / Sets Level Interrupt HIGH
			break;

		case triggerType_t::T_BOTHEDGES:
			PINT->ISEL &= ~(1 << m_pintSource);			//	0: Edge Sensitive, 1: Level sensitive
			PINT->IENR |= (1 << m_pintSource);			//	0: Disables Rising Edge / Level Interrupt, 1: Enables
			PINT->IENF |= (1 << m_pintSource);			//	0: Disables Falling Edge Interrupt / Sets Level	Interrupt LOW, 1: Enables Falling Edge Interrupt / Sets Level Interrupt HIGH
			break;

		case triggerType_t::T_LOWLVL:
			PINT->ISEL |= (1 << m_pintSource);			//	0: Edge Sensitive, 1: Level sensitive
			PINT->IENR |= (1 << m_pintSource);			//	0: Disables Rising Edge / Level Interrupt, 1: Enables
			PINT->IENF &= ~(1 << m_pintSource);			//	0: Disables Falling Edge Interrupt / Sets Level	Interrupt LOW, 1: Enables Falling Edge Interrupt / Sets Level Interrupt HIGH
			break;

		case triggerType_t::T_HIGHLVL:
			PINT->ISEL |= (1 << m_pintSource);			//	0: Edge Sensitive, 1: Level sensitive
			PINT->IENR |= (1 << m_pintSource);			//	0: Disables Rising Edge / Level Interrupt, 1: Enables
			PINT->IENF |= (1 << m_pintSource);			//	0: Disables Falling Edge Interrupt / Sets Level	Interrupt LOW, 1: Enables Falling Edge Interrupt / Sets Level Interrupt HIGH
			break;

		case triggerType_t::T_NONE:
			PINT->IENR &= ~(1 << m_pintSource);			//	0: Disables Rising Edge / Level Interrupt, 1: Enables
			break;

		default:
			break;
	}
	PINT->IST = (1 << m_pintSource);	//	Clears PININTn interrupt
}

Pint::pintSource_t Pint::getSource(void) const{ return m_pintSource; }

bool Pint::getDmaTrigger(InMux::DMA_TRIGGER_INPUT_SOURCE_t &trigger) const{
	if(m_pintSource < pintSource_t::PINTSEL4)	return false;

	trigger = (InMux::DMA_TRIGGER_INPUT_SOURCE_t)(InMux::PININT4 + (m_pintSource - 4));

	return true;
}

void PININT0_IRQHandler(void){
	PINT->IST = (1 << 0);	//	Clears PININT0 interrupt
	if(pinInt_instance[0] != nullptr)	pinInt_instance[0]->m_handler();
}

void PININT1_IRQHandler(void){
	PINT->IST = (1 << 1);	//	Clears PININT1 interrupt
	if(pinInt_instance[1] != nullptr) pinInt_instance[1]->m_handler();
}

void PININT2_IRQHandler(void){
	PINT->IST = (1 << 2);	//	Clears PININT2 interrupt
	if(pinInt_instance[2] != nullptr) pinInt_instance[2]->m_handler();
}

void PININT3_IRQHandler(void){
	PINT->IST = (1 << 3);	//	Clears PININT3 interrupt
	if(pinInt_instance[3] != nullptr) pinInt_instance[3]->m_handler();
}

void PININT4_IRQHandler(void){
	PINT->IST = (1 << 4);	//	Clears PININT4 interrupt
	if((pinInt_instance[4] != nullptr) && (pinInt_instance[4]->m_handler != nullptr))
		pinInt_instance[4]->m_handler();	//	DMA-Only Pint Can Have NO Handler
}

void PININT5_IRQHandler(void){
	PINT->IST = (1 << 5);	//	Clears PININT5 interrupt
	if((pinInt_instance[5] != nullptr) && (pinInt_instance[5]->m_handler != nullptr))
		pinInt_instance[5]->m_handler();	//	DMA-Only Pint Can Have NO Handler
}

void PININT6_IRQHandler(void){
	PINT->IST = (1 << 6);	//	Clears PININT6 interrupt
	if((pinInt_instance[6] != nullptr) && (pinInt_instance[6]->m_handler != nullptr))
		pinInt_instance[6]->m_handler();	//	DMA-Only Pint Can Have NO Handler
}

void PININT7_IRQHandler(void){
	PINT->IST = (1 << 7);	//	Clears PININT7 interrupt
	if((pinInt_instance[7] != nullptr) && (pinInt_instance[7]->m_handler != nullptr))
		pinInt_instance[7]->m_handler();	//	DMA-Only Pint Can Have NO Handler
}

Pint::~Pint(){
	NVIC->ICER[0] = (1 << (PIN_INT0_IRQn + m_pintSource));	//	Disables NVIC interrupt for PININTn
	m_trigger = triggerType_t::T_NONE;

	Pint::configIntExt();

	pinInt_instance[m_pintSource] = nullptr;
}
