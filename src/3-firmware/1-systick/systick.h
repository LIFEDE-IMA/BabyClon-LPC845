/*
 * systick.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#ifndef SYSTICK_H_
#define SYSTICK_H_

#include "LPC845.h"
#include "timedperipheral.h"

#if defined (__cplusplus)
extern "C" {

void SysTick_Handler(void);

}
#endif

void SystickInit();	//	Initializes systick hardware

#endif /* SYSTICK_H_ */
