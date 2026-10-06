/*
 * ds18b20.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#ifndef DS18B20_H_
#define DS18B20_H_

#include "onewire.h"

class DS18B20{
	public:
		static const uint16_t CONVERSION_TIME_MS = 750;

	private:
		static const uint8_t CMD_CONVERT_T = 0x44;		//	Initiates temperature conversion
		static const uint8_t CMD_READ_SCRATCH = 0xBE;	//	Reads the entire scratchpad (including the CRC byte)

		enum dsState_t : uint8_t{
			DS_IDLE = 0,
			DS_READING,
			DS_OK,
			DS_ERROR
		};

		dsState_t m_state;

		OneWire &m_owBus;

		uint8_t m_rom[8];
		bool m_hasRomFlag;
		uint8_t m_scratchPad[9];
		int16_t m_tempRaw;
		bool m_tempRdyFlag;

	public:
		DS18B20(OneWire &bus, const uint8_t *rom = nullptr);	//	Constructor

		bool setROM(const uint8_t *rom);		//	Returns False if [rom] is null or busBusy

		static bool startTempConversion(OneWire &bus);	//	Starts one-wire temp conversion for all DS18B20 on the bus. False if busBusy

		bool startTempReading(void);	//	Returns False if busBusy or No ROM
		bool tempRdy(void) const;		//	Returns true if temp was read
		int16_t getTempRaw(void) const;	//	Gets one slave temperature (no float)
		float getTemp(void) const;		//	Returns Last Valid Value raw/16

		bool hasError(void) const;		//	Returns True if [m_state] == DS_ERROR

		void processData(void);			//	Analyzes Scratchpad and OneWire Op

		~DS18B20();	//	Destructor
};

#endif /* DS18B20_H_ */
