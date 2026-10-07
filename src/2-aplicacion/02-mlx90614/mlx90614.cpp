/*
 * mlx90614.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "mlx90614.h"

const float MLX90614::GAIN_TABLE[8] = { 1.0f, 3.0f, 6.0f, 12.5f, 25.0f, 50.0f, 100.0f, 100.0f };

MLX90614::MLX90614(I2C &i2cMaster, uint8_t address, bool dualZone)
: m_i2cMaster(i2cMaster), m_delayTimer(MLX90614::EEPROM_DELAY_MS, SysTimer::SINGLE, SysTimer::T_MILI){
	m_addr = (address & 0x7F);
	m_dualFlag = dualZone;
	m_twoChannelsFlag = false;
	m_sleepingFlag = false;
	m_sleepJobFlag = false;

	m_mlxState = mlxState_t::MLX_IDLE;
	m_mlxStatus = mlxStatus_t::IDLE;
	m_i2cStatus = I2C::BUS_IDLE;

	m_ambTemp = m_obj1Temp = 0;
	m_obj2Temp = -1.0f;
	m_rawIR1 = 0;
	m_rawIR2 = -1;
	m_emissivityRaw = 0xFFFF;

	m_config = 0;
	m_eepromData = 0;
	m_readCount = m_readIndex = 0;
	m_stateAfterReading = mlxState_t::MLX_IDLE;
	m_word = 0;

	m_eepromAddr = 0;
	m_eepromMask = m_eepromValue = m_eepromNewValue = 0;
	m_writeTries = 0;
}

uint8_t MLX90614::crc8(const uint8_t *data, uint8_t len){
	uint8_t crc = 0;

	for(uint8_t idx = 0; idx < len; idx++){
		crc ^= data[idx];

		for(uint8_t bit = 0; bit < 8; bit++){
			if(crc & 0x80)
				crc = (uint8_t)((crc << 1) ^ 0x07);
			else
				crc = (uint8_t)(crc << 1);
		}
	}
	return crc;
}

bool MLX90614::readTamb(void){
	bool started = false;

	if(MLX90614::canStart()){
		uint8_t cmds[1] = {(uint8_t)(MLX90614::CMD_RAM | ramAddress_t::RAM_TAMB)};

		MLX90614::startEEPROMreading(cmds, 1, mlxState_t::MLX_IDLE);
		started = true;
	}
	return started;
}

bool MLX90614::readTobj(void){
	bool started = false;

	if(MLX90614::canStart()){
		uint8_t cmds[1] = {(uint8_t)(MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ)};

		MLX90614::startEEPROMreading(cmds, 1, mlxState_t::MLX_IDLE);
		started = true;
	}
	return started;
}

bool MLX90614::readTobj2(void){
	bool started = false;

	if(MLX90614::canStart()){
		uint8_t cmds[1] = {(uint8_t)(MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ2)};

		MLX90614::startEEPROMreading(cmds, 1, mlxState_t::MLX_IDLE);
		started = true;
	}
	return started;
}

bool MLX90614::readFullTemp(void){
	bool started = false;

	if(MLX90614::canStart()){
		uint8_t cmds[3] = {
				(uint8_t)(MLX90614::CMD_RAM | ramAddress_t::RAM_TAMB),
				(uint8_t)(MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ),
				(uint8_t)(MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ2)
		};

		uint8_t count = m_dualFlag ? 3 : 2;
		MLX90614::startEEPROMreading(cmds, count, mlxState_t::MLX_IDLE);
		started = true;
	}
	return started;
}

float MLX90614::getTamb(void) const{ return m_ambTemp; }

float MLX90614::getTobj(void) const{ return m_obj1Temp; }

float MLX90614::getTobj2(void) const{ return m_obj2Temp; }

bool MLX90614::readRawIR(uint8_t channel){
	bool started = false;

	if(((channel == 1) || (channel == 2)) && MLX90614::canStart()){
		uint8_t ramAddr = (channel == 1) ? MLX90614::RAM_RAW_IR1 : MLX90614::RAM_RAW_IR2;
		uint8_t cmds[1] = {(uint8_t)(MLX90614::CMD_RAM | ramAddr)};

		MLX90614::startEEPROMreading(cmds, 1, mlxState_t::MLX_IDLE);
		started = true;
	}
	return started;
}

int16_t MLX90614::getRawIR(uint8_t channel) const{
	if(channel == 1)
		return m_rawIR1;
	else if(channel = 2)
		return m_rawIR2;
	else
		return -1;
}

bool MLX90614::readEmissivity(void){
	return MLX90614::readEEPROM(eepromAddress_t::EE_EMISSIVITY);
}

float MLX90614::getEmissivity() const{ return (m_emissivityRaw / 65535.0f); }

bool MLX90614::readConfig(void){
	return MLX90614::readEEPROM(eepromAddress_t::EE_CONFIG1);
}

MLX90614::IIR_t MLX90614::getIIR(void) const{ return (IIR_t)(m_config & 0x7); }

uint16_t MLX90614::getFIRsamples(void) const{ return (8 << ((m_config >> 8) & 0x7)); }

float MLX90614::getGain(void) const{
	return MLX90614::GAIN_TABLE[(m_config >> 11) & 0x07];
}

bool MLX90614::isDualZone(void) const{ return m_dualFlag; }

bool MLX90614::hasTwoChannels(void) const{ return m_twoChannelsFlag; }

bool MLX90614::setFilter(IIR_t iirPercentage){
	bool started = false;

	if(MLX90614::canStart()){
		uint16_t value = (uint16_t)(iirPercentage);
		MLX90614::startEEPROMwriting(eepromAddress_t::EE_CONFIG1, MLX90614::CFG_IIR_MASK, value);
		started = true;
	}
	return started;
}

bool MLX90614::setFilter(FIR_t firSamples){
	bool started = false;

	if(MLX90614::canStart()){
		uint16_t value = (uint16_t)(firSamples << 8);
		MLX90614::startEEPROMwriting(eepromAddress_t::EE_CONFIG1, MLX90614::CFG_FIR_MASK, value);
		started = true;
	}
	return started;
}

bool MLX90614::setBusAddress(uint8_t newAddress){
	bool started = false;

	if((newAddress != 0) && (newAddress < 0x07) && MLX90614::canStart()){
		MLX90614::startEEPROMwriting(eepromAddress_t::EE_SMBUS_ADDR, 0x00FF, newAddress);
		started = true;
	}
	return started;
}

void MLX90614::updateBussAddress(uint8_t newAddress){ m_addr = (newAddress & 0x7F); }

bool MLX90614::readEEPROM(eepromAddress_t eepromAddress){
	bool started = false;

	if((eepromAddress <= 0x1F) && MLX90614::canStart()){
		uint8_t cmds[1] = {(uint8_t)(MLX90614::CMD_EEPROM | eepromAddress)};
		MLX90614::startEEPROMreading(cmds, 1, mlxState_t::MLX_IDLE);
		started = true;
	}
	return started;
}

uint16_t MLX90614::getEEPROMdata() const{ return m_eepromData; }

bool MLX90614::writeEEPROM(eepromAddress_t eepromAddress, uint16_t value){
	bool started = false;
	bool allowed = ((eepromAddress <= eepromAddress_t::EE_TARANGE) || (eepromAddress == eepromAddress_t::EE_SMBUS_ADDR));

	if(allowed && MLX90614::canStart()){
		MLX90614::startEEPROMwriting(eepromAddress, 0xFFFF, value);
		started = true;
	}
	return started;
}

uint16_t MLX90614::temp2raw(float celsius){
	float raw = (((celsius + 273.15f) * 100.0f) + 0.5f);
	uint16_t res = 0;

	if(raw >= 65535.0f)	res = 0xFFFF;
	else if(raw > 0.0f)	res = (uint16_t)raw;

	return res;
}

bool MLX90614::sleep(void){
	bool started = false;

	if(MLX90614::canStart()){
		m_sleepJobFlag = true;
		m_mlxStatus = mlxStatus_t::BUSY;
		m_mlxState = mlxState_t::MLX_SLEEP_REQUEST;
		started = true;
	}
	return started;
}

void MLX90614::setAwake(void){ m_sleepingFlag = false; }

bool MLX90614::isSleeping(void) const{ return m_sleepingFlag; }

bool MLX90614::isBusy(void) const{ return (m_mlxStatus == mlxStatus_t::BUSY); }

bool MLX90614::isDone(void) const{ return (m_mlxStatus == mlxStatus_t::OP_DONE); }

bool MLX90614::hasError(void) const{
	return ((m_mlxStatus >= mlxStatus_t::ERR_I2C) && (m_mlxStatus <= mlxStatus_t::ERR_PARAM));
}

MLX90614::mlxStatus_t MLX90614::getStatus(void) const{ return m_mlxStatus; }

I2C::i2cStatus_t MLX90614::getI2Cstatus(void) const{ return m_i2cStatus; }



//******************************************************************//
//	---------------------	STATE MACHINE	---------------------	//
//******************************************************************//


void MLX90614::stateMachine(void){
	switch(m_mlxState){
		case mlxState_t::MLX_IDLE:
			//	Nothing To Do
			break;

		case mlxState_t::MLX_READ_REQUEST:
			if(m_i2cMaster.readRegister(m_addr, m_readCmdList[m_readIndex], m_rxBuff, 3))
				m_mlxState = mlxState_t::MLX_READ_WAIT;
			break;

		case mlxState_t::MLX_READ_WAIT:
			if(!m_i2cMaster.isBusy())
				MLX90614::readFinished();
			break;

		case mlxState_t::MLX_EE_MODIFY:
			MLX90614::eepromModify();
			break;

		case mlxState_t::MLX_EE_ERASE_REQUEST:{
			uint8_t cmd = (uint8_t)(MLX90614::CMD_EEPROM | m_eepromAddr);
			MLX90614::armWrite(cmd, 0x0000);
			if(m_i2cMaster.write(m_addr, m_txBuff, 4))
				m_mlxState = mlxState_t::MLX_EE_ERASE_WAIT;
			break;
		}

		case mlxState_t::MLX_EE_ERASE_WAIT:
			if(!m_i2cMaster.isBusy()){
				if(m_i2cMaster.getStatus() == I2C::BUS_OP_DONE){
					MLX90614::startDelay();
					m_mlxState = mlxState_t::MLX_EE_ERASE_DELAY;
				}else{
					MLX90614::failI2C();
				}
			}
			break;

		case mlxState_t::MLX_EE_ERASE_DELAY:
			if(m_delayTimer.singleTimerExpired()){
				m_delayTimer.stopTimer();
				m_mlxState = mlxState_t::MLX_EE_WRITE_REQUEST;
			}
			break;

		case mlxState_t::MLX_EE_WRITE_REQUEST:{
			uint8_t cmd = (uint8_t)(MLX90614::CMD_EEPROM | m_eepromAddr);
			MLX90614::armWrite(cmd, m_eepromNewValue);
			if(m_i2cMaster.write(m_addr, m_txBuff, 4))
				m_mlxState = mlxState_t::MLX_EE_WRITE_WAIT;
			break;
		}

		case mlxState_t::MLX_EE_WRITE_WAIT:
			if(!m_i2cMaster.isBusy()){
				if(m_i2cMaster.getStatus() == I2C::BUS_OP_DONE){
					MLX90614::startDelay();
					m_mlxState = mlxState_t::MLX_EE_WRITE_DELAY;

				}else{	//	Cell Was Erased so Better Get that Write xd
					m_writeTries++;

					if(m_writeTries <= MLX90614::MAX_WRITE_RETRIES){
						MLX90614::startDelay();
						m_mlxState = mlxState_t::MLX_EE_RETRY_DELAY;
					}else{
						MLX90614::failI2C();
					}
				}
			}
			break;

		case mlxState_t::MLX_EE_WRITE_DELAY:
			if(m_delayTimer.singleTimerExpired()){
				m_delayTimer.stopTimer();

				uint8_t cmd = (uint8_t)(MLX90614::CMD_EEPROM | m_eepromAddr);
				MLX90614::startEEPROMreading(&cmd, 1, mlxState_t::MLX_EE_VERIFY);	//	Read Back
			}
			break;

		case mlxState_t::MLX_EE_RETRY_DELAY:
			if(m_delayTimer.singleTimerExpired()){
				m_delayTimer.stopTimer();
				m_mlxState = mlxState_t::MLX_EE_WRITE_REQUEST;
			}
			break;

		case mlxState_t::MLX_EE_VERIFY:
			if(m_word == m_eepromNewValue)
				MLX90614::finish(mlxStatus_t::OP_DONE);
			else
				MLX90614::finish(mlxStatus_t::ERR_VERIFY);
			break;

		case mlxState_t::MLX_SLEEP_REQUEST:{
			uint8_t pecData[2] = {(uint8_t)(m_addr << 1), MLX90614::CMD_SLEEP};

			m_txBuff[0] = MLX90614::CMD_SLEEP;
			m_txBuff[1] = MLX90614::crc8(pecData, 2);
			if(m_i2cMaster.write(m_addr, m_txBuff, 2))
				m_mlxState = mlxState_t::MLX_SLEEP_WAIT;
			break;
		}

		case mlxState_t::MLX_SLEEP_WAIT:
			if(!m_i2cMaster.isBusy()){
				if(m_i2cMaster.getStatus() == I2C::BUS_OP_DONE)
					MLX90614::finish(mlxStatus_t::OP_DONE);
				else
					MLX90614::failI2C();
			}
			break;

		default:
			//	ERROR
			MLX90614::finish(mlxStatus_t::ERR_I2C);
			break;
	}
}

//	HELPERS
bool MLX90614::canStart(void) const{
	return ((m_mlxState == mlxState_t::MLX_IDLE) && !m_sleepingFlag);
}

void MLX90614::startEEPROMreading(const uint8_t *cmds, uint8_t count, mlxState_t stateAfter){
	for(uint8_t idx = 0; idx < count; idx++)	m_readCmdList[idx] = cmds[idx];

	m_readCount = count;
	m_readIndex = 0;
	m_stateAfterReading = stateAfter;
	m_mlxStatus = mlxStatus_t::BUSY;
	m_mlxState = mlxState_t::MLX_READ_REQUEST;
}

void MLX90614::startEEPROMwriting(uint8_t eepromAddr, uint16_t mask, uint16_t value){
	uint8_t cmd = (uint8_t)(MLX90614::CMD_EEPROM | eepromAddr);

	m_eepromAddr = eepromAddr;
	m_eepromMask = mask;
	m_eepromValue = value;

	MLX90614::startEEPROMreading(&cmd, 1, mlxState_t::MLX_EE_MODIFY);
}

void MLX90614::armWrite(uint8_t cmd, uint16_t value){
	uint8_t pecData[4];

	m_txBuff[0] = cmd;
	m_txBuff[1] = (uint8_t)(value & 0xFF);
	m_txBuff[2] = (uint8_t)(value >> 8);

	//	CMD, LSB, MSB, PEC
	pecData[0] = (uint8_t)(m_addr << 1);
	pecData[1] = m_txBuff[0];
	pecData[2] = m_txBuff[1];
	pecData[3] = m_txBuff[2];
	m_txBuff[3] = MLX90614::crc8(pecData, 4);
}

void MLX90614::startDelay(void){
	m_delayTimer.stopTimer();
	m_delayTimer.startTimer();
}

void MLX90614::readFinished(void){
	I2C::i2cStatus_t i2cStatus = m_i2cMaster.getStatus();

	if(i2cStatus != I2C::BUS_OP_DONE){
		MLX90614::failI2C();
	}else{
		uint8_t cmd = m_readCmdList[m_readIndex];
		uint8_t pecData[5] = {
				(uint8_t)(m_addr << 1),
				cmd,
				(uint8_t)((m_addr << 1) | 1),
				m_rxBuff[0],
				m_rxBuff[1]
		};
		if(MLX90614::crc8(pecData, 5) != m_rxBuff[2]){
			MLX90614::finish(mlxStatus_t::ERR_PEC);
		}else{
			m_word = (uint16_t)(m_rxBuff[0] | (m_rxBuff[1] << 8));

			if(MLX90614::storeRead(cmd)){
				m_readIndex++;

				if(m_readIndex < m_readCount)
					m_mlxState = mlxState_t::MLX_READ_REQUEST;
				else if(m_stateAfterReading == mlxState_t::MLX_IDLE)
					MLX90614::finish(mlxStatus_t::OP_DONE);
				else
					m_mlxState = m_stateAfterReading;
			}
		}
	}
}

bool MLX90614::storeRead(uint8_t cmd){
	bool ok = true;

	switch(cmd){
		case (MLX90614::CMD_RAM | ramAddress_t::RAM_TAMB):
		case (MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ):
		case (MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ2):	//	Any of These Cases
			if(m_word & MLX90614::RAM_ERROR_FLAG){
				MLX90614::finish(mlxStatus_t::ERR_BAD_T);
				ok = false;
			}else{
				float celsius = (((float)m_word * 0.02f) - 273.15f);

				if(cmd == (MLX90614::CMD_RAM | ramAddress_t::RAM_TAMB))
					m_ambTemp = celsius;
				else if(cmd == (MLX90614::CMD_RAM | ramAddress_t::RAM_TOBJ))
					m_obj1Temp = celsius;
				else
					m_obj2Temp = celsius;
			}
			break;

		case (MLX90614::CMD_RAM | ramAddress_t::RAM_RAW_IR1):
		case (MLX90614::CMD_RAM | ramAddress_t::RAM_RAW_IR2):{
			int16_t magnitude = (int16_t)(m_word & 0x7FFF);
			int16_t value = (m_word & 0x8000) ? (int16_t)(-magnitude) : magnitude;

			if(cmd == (MLX90614::CMD_RAM | ramAddress_t::RAM_RAW_IR1))
				m_rawIR1 = value;
			else
				m_rawIR2 = value;
			break;
		}

		case (MLX90614::CMD_EEPROM | eepromAddress_t::EE_EMISSIVITY):
			m_emissivityRaw = m_word;
			m_eepromData = m_word;
			break;

		case (MLX90614::CMD_EEPROM | eepromAddress_t::EE_CONFIG1):
			m_config = m_word;
			m_twoChannelsFlag = ((m_word & MLX90614::CFG_DUAL_MASK) != 0);
			m_eepromData = m_word;
			break;

		default:
			m_eepromData = m_word;
			break;

	}

	return ok;
}

void MLX90614::eepromModify(void){
	uint16_t newValue = (uint16_t)((m_word & ~m_eepromMask) | (m_eepromValue & m_eepromMask));

	if(newValue == m_word){	//	Nothing To Do
		MLX90614::finish(mlxStatus_t::OP_DONE);
	}else{
		m_eepromNewValue = newValue;
		m_writeTries = 0;
		m_mlxState = mlxState_t::MLX_EE_ERASE_REQUEST;
	}
}

void MLX90614::failI2C(void){
	m_i2cStatus = m_i2cMaster.getStatus();
	MLX90614::finish(mlxStatus_t::ERR_I2C);
}

void MLX90614::finish(mlxStatus_t status){
	if((status == mlxStatus_t::OP_DONE) && m_sleepJobFlag)
		m_sleepingFlag = true;

	m_delayTimer.stopTimer();
	m_sleepJobFlag = false;
	m_mlxStatus = status;
	m_mlxState = mlxState_t::MLX_IDLE;
}

MLX90614::~MLX90614(){}
