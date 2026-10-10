/*
 * hwinit.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  LPC845 crucial hardware initialization ( 30MHz clock & SysTick )
 */

#ifndef HWINIT_H_
#define HWINIT_H_

#include "dr_pll.h"
#include "systick.h"

#define SHPR3 (*((volatile uint32_t *) 0xE000ED20))	//	ARM Cortex-M0+ System Handler Preiority

void HW_init();

#endif /* HWINIT_H_ */
