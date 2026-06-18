/*
 * mlx90614.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include <2-mlx90614/mlx90614.h>

MLX90614::MLX90614(I2C &i2cMaster) : m_i2cMaster(i2cMaster){
	m_objTemp = m_ambTemp = 0;
	m_objTempRdy = m_ambTempRdy = false;
	m_pendingRead = pendingRead_t::NONE;
}

void MLX90614::startAmbTempReading(){
	if(m_pendingRead != pendingRead_t::NONE)
		return;	//	Protects reading to be started while other is already in progress

	m_pendingRead = pendingRead_t::AMBIENT;
	m_ambTempRdy = false;

	m_i2cMaster.startReading(MLX90614_ADDR, MLX90614_REG_TAMB);
}

void MLX90614::startObjTempReading(){
	if(m_pendingRead != pendingRead_t::NONE)
		return;	//	Protects reading to be started while other is already in progress

	m_pendingRead = pendingRead_t::OBJECT;
	m_objTempRdy = false;

	m_i2cMaster.startReading(MLX90614_ADDR, MLX90614_REG_TOBJ1);
}

void MLX90614::updateStatus(){
	if(!m_i2cMaster.isDataRdy())
		return;	//	No data yet

	float temp = ((m_i2cMaster.getData() * 0.02f) - 273.15f);

	switch(m_pendingRead){
		case pendingRead_t::AMBIENT:
			m_ambTemp = temp;
			m_ambTempRdy = true;
			break;

		case pendingRead_t::OBJECT:
			m_objTemp = temp;
			m_objTempRdy = true;
			break;

		default:
			//	ERROR
			break;
	}

	m_pendingRead = pendingRead_t::NONE;
}

bool MLX90614::isAmbTempRdy(){ return m_ambTempRdy; }

bool MLX90614::isObjTempRdy(){ return m_objTempRdy; }

float MLX90614::getAmbTemp(){ return m_ambTemp; }

float MLX90614::getObjTemp(){ return m_objTemp; }

MLX90614::~MLX90614(){}
