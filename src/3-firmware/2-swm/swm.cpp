/*
 * swm.cpp
 *
 *  Created on: 18 jun. 2026
 *      Author: Julián - LIFEDE - UTN FRBA
 *      Consultas: jgross@frba.utn.edu.ar
 *
 *	This code was written to handle SPI class (and others if wanted)
 */

#include "swm.h"

int f;

void PINASSIGN_Config( uint8_t pin_movible , uint8_t port , uint8_t pin )
{
	if ( port == 1 )
		pin = pin + 32;

	PIN_ASSIGN[ pin_movible / 4 ] &= ~(0xff << (( pin_movible % 4 ) * 8 ));
	PIN_ASSIGN[ pin_movible / 4 ] |= pin << (( pin_movible % 4 ) * 8 );
}

void PINENABLE_Config( uint8_t pin_config , uint8_t enable )
{
	// TIENE LOGICA NEGADA, ES ACTIVO BAJO
	if ( !enable )
		PINENABLE[ pin_config / 32 ] |=  1 << ( pin_config % 32 ) ;
	else
		PINENABLE[ pin_config / 32 ] &= ~( 1 <<  ( pin_config % 32 ) );

}

