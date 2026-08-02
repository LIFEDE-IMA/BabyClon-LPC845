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

void HW_init(){
	PLL_init();
	SystickInit();
}

