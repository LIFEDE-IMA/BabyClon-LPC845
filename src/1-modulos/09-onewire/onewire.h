/*
 * onewire.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 *  Currently, it does NOT work by interrupts, only by polling (10 Jun 2026)
 */

#ifndef ONEWIRE_H_
#define ONEWIRE_H_

#include "ctimer.h"

#define CMD_SKIP_ROM      0xCC			//	Address all devices on the bus simultaneously
#define CMD_MATCH_ROM     0x55			//	Address a specific slave device on the bus
#define CMD_READ_ROM	  0x33			//	Reads only one slave's 64-bit ROM
#define CMD_SEARCH_ROM	  0xF0			//	Identifies all slave devices on the bus

#define MAX_SLAVES	5	//	Defines the maximum number of slaves per bus

class OneWire{
	private:
		uint32_t m_wantedTime;
		volatile bool m_presence;		//	Presence = true, if slave sets line LOW
		volatile bool m_multiDataRdy;
		volatile bool m_ROMsRdy;		//	SEARCH ROM flag
		volatile bool m_bit;
		uint8_t m_byte;
		uint8_t m_bitIndex;
		uint8_t m_lastDiscrepancyBit;
		uint8_t m_currentDiscrepancyBit;
		uint8_t m_bitNumber;
		uint8_t m_byteNumber;
		uint8_t m_bitMask;
		volatile bool m_idBit;
		volatile bool m_ctoBit;
		volatile bool m_searchDirection;
		volatile bool m_lastDevice;			//	SEARCH ROM flag
		uint8_t m_allROMs[MAX_SLAVES][8];	//	Each slave ROM
		uint8_t m_rom[8];					//	Current rom being builded in search rom
		uint8_t m_slvsNumber;
		volatile uint32_t m_startTime;

		void init();										//	Initialization
		void usDelay(uint32_t t);							//	Sets a us delay

	public:
		OneWire();											//	Constructor

		void setBus_low();									//	Sets line LOW
		void releaseBus();									//	Sets line free (pull-up R => High)
		bool readBus();										//	Returns bus state (LOW / HIGH)

		void startBusReset();								//	Starts bus reset
		void startBitReading();								//	Starts one bit reading
		void startByteReading();							//	Starts one byte reading
		void startBitWriting(bool bit);						//	Starts one bit writing
		void startByteWriting(uint8_t byte);				//	Starts one byte writing

		uint8_t getCRC(uint8_t *scratchpad, uint8_t len);		//	Gets crc byte

		void startROMsearch();								//	Searches all salve IDs connected to the bus
		bool areAllROMsRdy() const;							//	Returns true if all ROMs are ready
		uint8_t getSlvsNumber() const;						//	Returns the number of slaves connected

		uint8_t getByte() const;							//	Returns [m_byte]

		~OneWire();											//	Destructor
};

#endif /* ONEWIRE_H_ */
