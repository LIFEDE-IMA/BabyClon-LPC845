/*
 * ds18b20.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 */

#include "ds18b20.h"

const uint8_t DS18B20::CMD_CONVERT_T;		//	Just Definitions, so their
const uint8_t DS18B20::CMD_READ_SCRATCH;	//	Address are Taken by startTransaction()

DS18B20::DS18B20(OneWire &bus, const uint8_t *rom) : m_owBus(bus){
	for(uint8_t idx = 0; idx < 8; idx++) m_rom[idx] = 0;
	for(uint8_t idx = 0; idx < 9; idx++) m_scratchPad[idx] = 0;
	m_hasRomFlag = false;
	m_tempRdyFlag = false;
	m_tempRaw = 0;
	m_state = dsState_t::DS_IDLE;

	if(rom != nullptr)	DS18B20::setROM(rom);
}

bool DS18B20::setROM(const uint8_t *rom){
	if((rom == nullptr) || (m_state == dsState_t::DS_READING))
		return false;

	for(uint8_t idx = 0; idx < 8; idx++)	m_rom[idx] = rom[idx];
	m_hasRomFlag = true;
	m_state = dsState_t::DS_IDLE;
	return true;
}

bool DS18B20::startTempConversion(OneWire &bus){
	return bus.startTransaction(OneWire::CMD_SKIP_ROM, nullptr, &DS18B20::CMD_CONVERT_T, 1, nullptr, 0);	// Send "instruction for everyone" command and Send "measure temperature" command
}

bool DS18B20::startTempReading(void){
	if(!m_hasRomFlag || (m_state == dsState_t::DS_READING))
		return false;

	if(!m_owBus.startTransaction(OneWire::CMD_MATCH_ROM, m_rom, &DS18B20::CMD_READ_SCRATCH, 1, m_scratchPad, 9))	//	Read ScratchPad of [m_rom]
		return false;

	m_tempRdyFlag = false;
	m_state = dsState_t::DS_READING;
	return true;
}

bool DS18B20::tempRdy() const{ return m_tempRdyFlag; }

int16_t DS18B20::getTempRaw(void) const{ return m_tempRaw; }

float DS18B20::getTemp(void) const{ return (m_tempRaw / 16.0f); }

bool DS18B20::hasError(void) const{ return (m_state == dsState_t::DS_ERROR); }

void DS18B20::processData(void){
	if((m_state != dsState_t::DS_READING) || m_owBus.isBusy())
		return;

	if((m_owBus.getStatus() == OneWire::OP_DONE) && OneWire::isCRC8ok(m_scratchPad, 9)){
		uint8_t res = ((m_scratchPad[4] >> 5) & 0x3);	//	Resolution: 0 -> 9bit, ... , 3 -> 12bit
		int16_t rawTemp = (int16_t)(((uint16_t)m_scratchPad[1] << 8) | m_scratchPad[0]);	//	(msb << 8) | lsb

		rawTemp &= ~((1 << (3 - res)) - 1);
		m_tempRaw = rawTemp;
		m_tempRdyFlag = true;
		m_state = dsState_t::DS_OK;
	}else{
		m_state = dsState_t::DS_ERROR;
	}
}

DS18B20::~DS18B20(){}
