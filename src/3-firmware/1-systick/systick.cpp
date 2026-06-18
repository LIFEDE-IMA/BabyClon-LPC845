/*
 * systick.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#include "systick.h"

void SystickInit(){
	SYSTICK->CSR &= ~(1 << 0);	//	Systick Counter disabled

	SYSTICK->RVR = ((FREQ_CLOCK / 1000) - 1);	//	1ms	(-1 cause LPC needs one tick to reload)

	SYSTICK->CVR = SYSTICK->RVR;	//	First interrupt lasts same than next ones

	SYSTICK->CSR |= (1 << 2);	//	Clk_source = system_clk

	SYSTICK->CSR |= (1 << 1);	//	Interrupt enabled

	SYSTICK->CSR |= (1 << 0);	//	Systick Counter enabled
}

void SysTick_Handler(void){
	for(uint8_t idx = 0; idx < TimedPeripheral::timedPeripSources; idx++)
		timedPeripheral_instances[idx]->handler();
}
