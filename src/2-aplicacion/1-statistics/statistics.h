/*
 * statistics.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This class was written to contain statistics operations ( methods )
 */

#ifndef STATISTICS_H_
#define STATISTICS_H_

#include <stdint.h>

class Statistics{
	public:
		Statistics();

		static float median(const float *vec, uint8_t size);	//	Returns the median of a float vector

		~Statistics();
};


#endif /* STATISTICS_H_ */
