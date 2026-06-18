/*
 * ds18b20.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  Currently, it does NOT work by interrupts, only by polling (10 Jun 2026)
 */

#ifndef DS18B20_H_
#define DS18B20_H_

#include "onewire.h"

#define CMD_CONVERT_T     0x44			//	Initiates temperature conversion
#define CMD_READ_SCRATCH  0xBE			//	Reads the entire scratchpad (including the CRC byte)

class DS18B20{
	private:
		OneWire &m_owBus;

		uint8_t m_rom[8];
		uint8_t m_scratchPad[9];
		uint8_t m_crc;
		float m_temp;
		bool m_tempRdy;

	public:
		DS18B20(OneWire &bus, const uint8_t rom[8]);	//	Constructor

		static void startTempConversion(OneWire &bus);	//	Starts one-wire temp conversion for all DS18B20 on the bus

		float getTemp();	//	Gets one slave temperature

		bool isTempRdy() const;	//	Returns true if temp was read

		~DS18B20();	//	Destructor
};

#endif /* DS18B20_H_ */
