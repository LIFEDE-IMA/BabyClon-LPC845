/*
 * ds18b20.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  Currently, it does NOT work by interrupts, only by polling (10 Jun 2026)
 */

#include "ds18b20.h"

DS18B20::DS18B20(OneWire &bus, const uint8_t rom[8]) : m_owBus(bus){
	for(uint8_t idx = 0; idx < 8; idx++) m_rom[idx] = rom[idx];
	for(uint8_t idx = 0; idx < 9; idx++) m_scratchPad[idx] = 0;
	m_crc = 0;
	m_temp = 0;
	m_tempRdy = false;
}

void DS18B20::startTempConversion(OneWire &bus){
	bus.startBusReset();
	bus.startByteWriting(CMD_SKIP_ROM);		// Send "instruction for everyone" command
	bus.startByteWriting(CMD_CONVERT_T);	// Send "measure temperature" command
}

float DS18B20::getTemp(){
	int i;

	m_owBus.startBusReset();
	m_owBus.startByteWriting(CMD_MATCH_ROM); // Address a specific slave
	for(i = 0; i < 8; i++){
		m_owBus.startByteWriting(m_rom[i]); // Slave id
	}
	m_owBus.startByteWriting(CMD_READ_SCRATCH); // Read the scratchpad

	for(i = 0; i < 9; i++){
		m_owBus.startByteReading();
		m_scratchPad[i] = m_owBus.getByte();
	}
	m_crc = m_owBus.getCRC(m_scratchPad, 8); // Gets crc byte

	if(m_crc != m_scratchPad[8]){ // Compares with crc got from slave
		m_temp = -1.0;
		return m_temp; // Communication failure
	}
	m_temp = ((m_scratchPad[1] << 8) | m_scratchPad[0]); // (msb << 8) | lsb
	return (m_temp / 16.0);
}

bool DS18B20::isTempRdy() const{ return m_tempRdy; }

DS18B20::~DS18B20(){}
