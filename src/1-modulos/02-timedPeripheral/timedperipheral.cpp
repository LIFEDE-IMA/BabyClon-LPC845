/*
 * timedperipheral.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#include "timedperipheral.h"

TimedPeripheral **timedPeripheral_instances = nullptr;
uint8_t TimedPeripheral::timedPeripSources = 0;

TimedPeripheral::TimedPeripheral(){
	TimedPeripheral **aux = new TimedPeripheral*[timedPeripSources + 1];	//	Adds new peripheral to the array

	for(uint8_t idx = 0; idx < timedPeripSources; idx++)
		aux[idx] = timedPeripheral_instances[idx];

	aux[timedPeripSources] = this;
	timedPeripSources++;

	delete[] timedPeripheral_instances;
	timedPeripheral_instances = aux;
}

TimedPeripheral::~TimedPeripheral(){
	uint8_t index;

	for(index = 0; index < timedPeripSources; index++)
		if(timedPeripheral_instances[index] == this)
			break;	//	Finds current instance position

	TimedPeripheral **aux = new TimedPeripheral *[timedPeripSources - 1];

	for(uint8_t i = 0; i < index; i++)
		aux[i] = timedPeripheral_instances[i];
	for(uint8_t i = index + 1; i < timedPeripSources; i++)
		aux[i - 1] = timedPeripheral_instances[i];	//	Deletes current instance from the array

	delete[] timedPeripheral_instances;
	timedPeripSources--;
	timedPeripheral_instances = aux;
}

