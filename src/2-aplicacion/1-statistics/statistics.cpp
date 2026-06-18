/*
 * statistics.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This class was written to contain statistics operations ( methods )
 */

#include <1-statistics/statistics.h>

Statistics::Statistics(){}

float Statistics::median(const float *vec, uint8_t size){
	float *aux = new float[size];
	float largest, ret = 0;

	for(uint8_t i = 0; i < size; i++) aux[i] = vec[i];

	for(uint8_t i = 0; i < (size - 1); i++){
		for(uint8_t j = 0; j < (size - i - 1); j++){
			if(aux[j] > aux[j + 1]){
				largest = aux[j];
				aux[j] = aux[j + 1];
				aux[j + 1] = largest;
			}
		}
	}	//	The vec is ordered from smallest to largest

	if(size % 2){	//	Odd
		ret = aux[((size - 1) / 2)];
	}else{	//	Even
		ret = ((aux[((size - 1) / 2)] + aux[(size / 2)]) / 2.0f);
	}

	delete[] aux;

	return ret;	//	Returns the median from all values
}

Statistics::~Statistics(){}
