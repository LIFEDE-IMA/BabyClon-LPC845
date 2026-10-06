/*
 * mlx90614.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This class was written to handle MLX90614 temperature sensor with I2C peripheral
 */

#ifndef MLX90614_H_
#define MLX90614_H_

#include "i2c.h"

#define MLX90614_ADDR   	0x5A 	// Sensor Address
#define MLX90614_REG_TOBJ1  0x07 	// Object Temperature Address
#define MLX90614_REG_TAMB	0x06	// Ambient Temperature Address

class MLX90614{
	private:
		enum pendingRead_t{
			NONE = 0,
			AMBIENT,
			OBJECT
		};

		I2C &m_i2cMaster;

		float m_ambTemp;
		float m_objTemp;
		bool m_ambTempRdy;
		bool m_objTempRdy;

		pendingRead_t m_pendingRead;

	public:
		MLX90614(I2C &i2cMaster);	//	Constructor

		void startAmbTempReading();	//	Starts ambient temperature reading
		void startObjTempReading();	//	Starts object temperature reading
		void updateStatus();		//	Updates class MLX90614 internal status
		bool isAmbTempRdy();		//	Returns true if ambient temperature was acquired
		bool isObjTempRdy();		//	Returns true if object temperature was acquired
		float getAmbTemp();			//	Returns ambient temperature
		float getObjTemp();			//	Returns object temperature

		~MLX90614();	//	Destructor
};

#endif /* MLX90614_H_ */
