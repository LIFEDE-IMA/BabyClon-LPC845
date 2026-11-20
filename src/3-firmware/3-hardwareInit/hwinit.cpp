/*
 * hwinit.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  LPC845 crucial hardware initialization ( 30MHz clock & SysTick )
 */

#include "hwinit.h"

//	When Having Too Much Peripherals Running ISRs Uart Starts Losing Bytes
//	So Uart is Set as Highest Priority ( 0 ) in its Constructor
static void NVIC_setDefaultPriorities(void){
	for(uint8_t idx = 0; idx < 8; idx++){
		NVIC->IP[idx] &= ~((3UL << 6) | (3UL << 14) | (3UL << 22) | (3UL << 30));
		NVIC->IP[idx] |= ((1UL << 6) | (1UL << 14) | (1UL << 22) | (1UL << 30));	//	All Interrupts Priority = 1
	}

	SHPR3 &= ~(3UL << 30);
	SHPR3 |= (1UL << 30);	//	Systick Priority 1

	//	UARTs Stay at Highest Priority (0), Even if Their Constructors Ran First
	const uint8_t uartIrqs[] = {
		(uint8_t)(USART0_IRQn + 0),
		(uint8_t)(USART0_IRQn + 1),
		(uint8_t)(USART0_IRQn + 2),
		(uint8_t)(USART0_IRQn + 27),
		(uint8_t)(USART0_IRQn + 28)
	};
	for(uint8_t irq : uartIrqs)	//	Copy every [uartIrqs] into [irq]
		NVIC->IP[irq >> 2] &= ~(3UL << (((irq & 3) * 8) + 6));
}

void HW_init(){
	PLL_init();
	NVIC_setDefaultPriorities();
	SystickInit();
}

