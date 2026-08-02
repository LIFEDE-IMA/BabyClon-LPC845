/*
 * eth.cpp
 *
 *  Created on: 17 jul. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This class was written to handle W5500 Ethernet module with SPI peripheral
 *
 * 	Byte 1:	Address High
 * 	Byte 2: Address Low
 * 	Byte 3: Control Byte = BSB[7:3] OM[2] R/W[1:0]
 * 	Byte 4: Data
 */

#include "eth.h"

Eth::Eth(bool portCS, uint8_t pinCS, Spi &spi) : SpiSlave(portCS, pinCS, spi, Spi::SPI_SLAVE_SELECT_ACTIVE_LOW){
	m_pendingReadFlag = false;
	m_socketTransferDone = false;
	m_doneFlag = nullptr;
	m_readBuffer = nullptr;
	m_readLen = 0;
	m_ethState = ethState_t::ETH_IDLE;
}

void Eth::transfer(registerAddr_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode){
	m_txBuffer[0] = (addr >> 8);	//	Address High
	m_txBuffer[1] = (addr & 0xFF);	//	Address Low
	m_txBuffer[2] = ((block << 3) | (rwMode << 2) | (opMode << 0));

	for(uint16_t i = 0; i < len; i++){
		if(rwMode == rwMode_t::WRITE){
			m_txBuffer[(3 + i)] = data[i];
		}else{
			m_txBuffer[(3 + i)] = 0;	//	No data will be transmitted
		}
	}

	Transmit(m_txBuffer, m_rxBuffer, (len + 3), f_done);
}

void Eth::readByte(registerAddr_t addr, block_t block, volatile bool *f_done, opMode_t opMode){
	Eth::transfer(addr, block, rwMode_t::READ, nullptr, 1, f_done, opMode);
}

uint8_t Eth::readByteAndWait(registerAddr_t addr, block_t block, opMode_t opMode){
	volatile bool f_done = false;

	Eth::transfer(addr, block, rwMode_t::READ, nullptr, 1, &f_done, opMode);

	while(!f_done);

	return m_rxBuffer[3];
}

void Eth::writeByte(registerAddr_t addr, block_t block, uint8_t data, volatile bool *f_done, opMode_t opMode){
	Eth::transfer(addr, block, rwMode_t::WRITE, &data, 1, f_done, opMode);
}

void Eth::readBuffer(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode){
	m_readBuffer = buffer;	//	Saves user buffer addr
	m_readLen = len;
	m_doneFlag = f_done;
	m_pendingReadFlag = true;

	Eth::transfer(addr, block, rwMode_t::READ, nullptr, len, f_done, opMode);
}

void Eth::readBufferAndWait(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode){
	volatile bool f_done = false;

	readBuffer(addr, block, buffer, len, &f_done, opMode);

	while(!f_done);

	for(uint16_t i = 0; i < len; i++){
		buffer[i] = m_rxBuffer[(i+3)];
	}
}

void Eth::writeBuffer(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode){
	Eth::transfer(addr, block, rwMode_t::WRITE, buffer, len, f_done, opMode);
}

void Eth::writeBufferAndWait(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode){
	volatile bool f_done = false;

	Eth::writeBuffer(addr, block, buffer, len, &f_done, opMode);

	while(!f_done);
}

void Eth::setIP(uint8_t ip[4], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, ip, 4, f_done, opMode);
}

void Eth::setIPAndWait(uint8_t ip[4], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, ip, 4, opMode);
}

void Eth::readIP(uint8_t ip[4], opMode_t opMode){
	Eth::readBufferAndWait(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, ip, 4, opMode);
}

void Eth::setGateway(uint8_t gateway[4], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, gateway, 4, f_done, opMode);
}

void Eth::setGatewayAndWait(uint8_t gateway[4], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, gateway, 4, opMode);
}

void Eth::readGateway(uint8_t gateway[4], opMode_t opMode){
	Eth::readBufferAndWait(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, gateway, 4, opMode);
}

void Eth::setSubnet(uint8_t subnet[4], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, subnet, 4, f_done, opMode);
}

void Eth::setSubnetAndWait(uint8_t subnet[4], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, subnet, 4, opMode);
}

void Eth::readSubnet(uint8_t subnet[4], opMode_t opMode){
	Eth::readBufferAndWait(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, subnet, 4, opMode);
}

void Eth::setMAC(uint8_t mac[6], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, mac, 6, f_done, opMode);
}

void Eth::setMACAndWait(uint8_t mac[6], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, mac, 6, opMode);
}

void Eth::readMAC(uint8_t mac[6], opMode_t opMode){
	Eth::readBufferAndWait(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, mac, 6, opMode);
}

void Eth::socketOpenTCP(uint16_t port){
	m_socketPortBuffer[0] = (port >> 8);
	m_socketPortBuffer[1] = (port & 0xFF);

	m_ethState = ethState_t::ETH_SOCKET_WRITE_MODE;
}

void Eth::socketClose(){

}

Eth::socketStat_t Eth::socketStatus(){
	return (socketStat_t)Eth::readByteAndWait(registerAddr_t::Sn_SR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK);
}

void Eth::socketConnect(uint8_t ip[4], uint16_t port){

}

bool Eth::socketConnected(){
	return (Eth::socketStatus() == socketStat_t::SOCK_ESTABLISHED);
}

void Eth::handler(){
	if(m_pendingReadFlag){
		if(*m_doneFlag){
			for(uint16_t i = 0; i < m_readLen; i++){
				m_readBuffer[i] = m_rxBuffer[(i+3)];
			}
			m_pendingReadFlag = false;
			m_readBuffer = nullptr;
			m_doneFlag = nullptr;
			m_readLen = 0;
		}
	}

	//	Ethernet States Machine

	switch(m_ethState){
		case ethState_t::ETH_IDLE:
			break;

		case ethState_t::ETH_SOCKET_WRITE_MODE:
			m_socketTransferDone = false;

			Eth::writeByte(registerAddr_t::Sn_MR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketMode_t::TCP_MODE, &m_socketTransferDone);

			m_ethState = ethState_t::ETH_SOCKET_WAIT_WRITE_MODE;
			break;

		case ethState_t::ETH_SOCKET_WAIT_WRITE_MODE:
			if(m_socketTransferDone)	m_ethState = ethState_t::ETH_SOCKET_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_WRITE_PORT:
			m_socketTransferDone = false;

			Eth::writeBuffer(registerAddr_t::Sn_PORT0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_socketPortBuffer, 2, &m_socketTransferDone);

			m_ethState = ethState_t::ETH_SOCKET_WAIT_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_WAIT_WRITE_PORT:
			if(m_socketTransferDone)	m_ethState = ethState_t::ETH_SOCKET_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_WRITE_COMMAND:
			m_socketTransferDone = false;

			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::OPEN_SOCKET, &m_socketTransferDone);

			m_ethState = ethState_t::ETH_SOCKET_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone)	m_ethState = ethState_t::ETH_SOCKET_FINISHED;
			break;

		case ethState_t::ETH_SOCKET_FINISHED:
			m_ethState = ethState_t::ETH_IDLE;
			break;

		default:
			//	ERROR
			break;
	}

}

Eth::~Eth(){}
