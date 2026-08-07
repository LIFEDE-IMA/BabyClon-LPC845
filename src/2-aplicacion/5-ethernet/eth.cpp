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

Eth::Eth(bool portCS, uint8_t pinCS, Spi &spi) : SpiSlave(portCS, pinCS, spi, Spi::SPI_SLAVE_SELECT_ACTIVE_LOW), m_timeoutTimer(5, SysTimer::SINGLE, SysTimer::T_SEG){
	m_pendingReadFlag = false;
	m_socketTransferDone = false;
	m_doneFlag = nullptr;
	m_readBuffer = nullptr;
	m_readLen = 0;
	m_sendFinishedFlag = false;
	m_rcvFinishedFlag = false;
	m_initFinishedFlag = false;
	m_currentConfigStat = configState_t::CONFIG_NONE;
	m_socketStat = socketStat_t::SOCK_CLOSED;
	m_nextStateAfterStatusRead = ethState_t::ETH_IDLE;
	m_ethState = ethState_t::ETH_IDLE;
	m_ethError = ethErrorState_t::ERROR_NONE;
}

void Eth::transfer(uint16_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode){
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

void Eth::readByte(uint16_t addr, block_t block, volatile bool *f_done, opMode_t opMode){
	Eth::transfer(addr, block, rwMode_t::READ, nullptr, 1, f_done, opMode);
}

uint8_t Eth::readByteAndWait(uint16_t addr, block_t block, opMode_t opMode){
	volatile bool f_done = false;

	Eth::transfer(addr, block, rwMode_t::READ, nullptr, 1, &f_done, opMode);

	while(!f_done);

	return m_rxBuffer[3];
}

void Eth::writeByte(uint16_t addr, block_t block, uint8_t data, volatile bool *f_done, opMode_t opMode){
	Eth::transfer(addr, block, rwMode_t::WRITE, &data, 1, f_done, opMode);
}

void Eth::readBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode){
	if(block != block_t::SOCKET0_RX_BUFFER_BLOCK){
		m_readBuffer = buffer;	//	Saves user buffer addr
		m_readLen = len;
		m_doneFlag = f_done;
		m_pendingReadFlag = true;

		Eth::transfer(addr, block, rwMode_t::READ, nullptr, len, f_done, opMode);
		return;
	}
	uint16_t offset = addr & m_rxBufferMask;

	if((offset + len) <= m_rxBufferSize){
		m_readBuffer = buffer;	//	Saves user buffer addr
		m_readLen = len;
		m_doneFlag = f_done;
		m_pendingReadFlag = true;

		Eth::transfer(addr, block, rwMode_t::READ, nullptr, len, f_done, opMode);
		return;
	}else{	//	W5500 Circular Buffer
		m_pendingReadWrap = true;
		m_firstReadLen = (m_rxBufferSize - offset);
		m_secondReadLen = (len - m_firstReadLen);
		m_userReadBuffer = buffer;
		m_readBuffer = buffer;	//	Saves user buffer addr
		m_readLen = m_firstReadLen;
		m_doneFlag = f_done;
		m_pendingReadFlag = true;

		Eth::transfer(addr, block, rwMode_t::READ, nullptr, m_firstReadLen, f_done, opMode);
	}
}

void Eth::readBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode){
	volatile bool f_done = false;

	readBuffer(addr, block, buffer, len, &f_done, opMode);

	while(!f_done);

	for(uint16_t i = 0; i < len; i++){
		buffer[i] = m_rxBuffer[(i+3)];
	}
}

void Eth::writeBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode){
	if(block != block_t::SOCKET0_TX_BUFFER_BLOCK){
		Eth::transfer(addr, block, rwMode_t::WRITE, buffer, len, f_done, opMode);
		return;
	}

	uint16_t offset = addr & m_txBufferMask;

	if((offset + len) <= m_txBufferSize){
		Eth::transfer(addr, block, rwMode_t::WRITE, buffer, len, f_done, opMode);
	}else{	//	W5500 Circular Buffer
		uint16_t firstPacketLen = (m_txBufferSize - offset);
		uint16_t secondPacketLen = (len - firstPacketLen);

		Eth::transfer(addr, block, rwMode_t::WRITE, buffer, firstPacketLen, nullptr, opMode);
		Eth::transfer(0, block, rwMode_t::WRITE, buffer + firstPacketLen, secondPacketLen, f_done, opMode);
	}
}

void Eth::writeBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode){
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

void Eth::init(uint8_t ip[4], uint8_t gateway[4], uint8_t subnet[4], uint8_t mac[6], socketBufferSize_t rxBufferSize, socketBufferSize_t txBufferSize, socketCloseMode_t closeMode,opMode_t opMode){
	if(Eth::isBusy())
		return;

	m_socketCloseMode = closeMode;
	m_opMode = opMode;

	for(uint8_t i = 0; i < 4; i++){
		 m_ip[i] = ip[i];
		 m_gateway[i] = gateway[i];
		 m_subnet[i] = subnet[i];
	}
	for(uint8_t i = 0; i < 6; i++)	m_mac[i] = mac[i];

	m_rxBufferSize = (uint16_t)(rxBufferSize * 1024);
	m_rxBufferMask = m_rxBufferSize - 1;

	m_txBufferSize = (uint16_t)(txBufferSize * 1024);
	m_txBufferMask = m_txBufferSize - 1;

	m_initFinishedFlag = false;
	m_currentConfigStat = configState_t::CONFIG_IP;
	m_ethState = ethState_t::ETH_CONFIG_WRITE;
}

bool Eth::isBusy() const{
	return (m_ethState != ethState_t::ETH_IDLE);
}

bool Eth::isReady() const{
	return (!Eth::isBusy() && m_initFinishedFlag);
}

bool Eth::isLinkUp(){
	uint8_t PHY = Eth::readByteAndWait(registerAddr_t::PHYCFGR_REGISTER, block_t::COMMON_REGISTER_BLOCK, opMode_t::VAR_DATA_LEN);

	return (PHY & 0x01);
}

Eth::socketStat_t Eth::socketStatus() const{ return m_socketStat; }

void Eth::socketRequestStatus(ethState_t nextStateAfterStatusRead){
    m_nextStateAfterStatusRead = nextStateAfterStatusRead;
    m_ethState = ethState_t::ETH_SOCKET_STATUS_READ;
}

void Eth::timeoutError(){
	m_timeoutTimer.stopTimer();
	m_ethError = ethErrorState_t::ERROR_TIMEOUT;
	m_ethState = ethState_t::ETH_IDLE;
}

/*Eth::socketStat_t Eth::socketStatus(){
	return (socketStat_t)Eth::readByteAndWait(registerAddr_t::Sn_SR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK);
}
*/

void Eth::socketOpenTCP(uint16_t localPort){
	if(!Eth::isReady())
		return;

	m_localPortBuffer[0] = (localPort >> 8);
	m_localPortBuffer[1] = (localPort & 0xFF);

	m_ethState = ethState_t::ETH_SOCKET_OPEN_WRITE_MODE;
}

bool Eth::socketOpened() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_INIT));
}

void Eth::socketConnect(uint8_t remoteIP[4], uint16_t remotePort){
	if(!Eth::isReady())
		return;

	for(uint8_t i = 0; i < 4; i++)
		m_remoteIPBuffer[i] = remoteIP[i];

	m_remotePortBuffer[0] = (remotePort >> 8);
	m_remotePortBuffer[1] = (remotePort & 0xFF);

	m_ethState = ethState_t::ETH_SOCKET_CONNECT_WRITE_IP;
}

bool Eth::socketConnected() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_ESTABLISHED));
}

void Eth::socketSend(uint8_t *buffer, uint16_t len){
	if(!Eth::isReady())
		return;

	m_sendBuffer = buffer;
	m_sendLen = len;
	m_sendFinishedFlag = false;

	m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
}

bool Eth::socketSendFinished() const{
	return (!Eth::isBusy() && m_sendFinishedFlag);
}

void Eth::socketReceive(uint8_t *buffer, uint16_t maxLen){
	if(!Eth::isReady())
		return;

	m_rcvBuffer = buffer;
	m_usrAskedRcvLen = maxLen;
	m_actualRcvLen = 0;
	m_rcvFinishedFlag = false;

	m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;
}
uint16_t Eth::socketReceivedLen() const{ return m_actualRcvLen; }

bool Eth::socketReceiveFinished() const{
	return (!Eth::isBusy() && m_rcvFinishedFlag);
}

void Eth::socketDisconnect(){
	if(!Eth::isReady())
		return;

	m_ethState = ethState_t::ETH_SOCKET_DISCONNECT_WRITE_COMMAND;
}

bool Eth::socketDisconnectFinished() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::socketClose(){
	if(!Eth::isReady())
		return;

	m_ethState = ethState_t::ETH_SOCKET_CLOSE_WRITE_COMMAND;
}

bool Eth::socketCloseFinished() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::handler(){
	if(m_pendingReadFlag){
		if(*m_doneFlag){
			for(uint16_t i = 0; i < m_readLen; i++){
				m_readBuffer[i] = m_rxBuffer[(i+3)];
			}
			if(m_pendingReadWrap){	//	W5500 Circular Buffer
				m_pendingReadWrap = false;
				m_readBuffer = (m_userReadBuffer + m_firstReadLen);
				m_readLen = m_secondReadLen;

				Eth::transfer(0, block_t::SOCKET0_RX_BUFFER_BLOCK, rwMode_t::READ, nullptr, m_secondReadLen, m_doneFlag);

				return;
			}
			m_pendingReadFlag = false;
			m_pendingReadWrap = false;
			m_readBuffer = nullptr;
			m_userReadBuffer = nullptr;
			m_doneFlag = nullptr;
			m_readLen = 0;
			m_firstReadLen = 0;
			m_secondReadLen = 0;
		}
	}

	// Ethernet States Machine

	switch(m_ethState){
		case ethState_t::ETH_IDLE:
			if(m_socketCloseMode == socketCloseMode_t::AUTO_CLOSE && !m_pendingReadFlag){
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_STATUS_CHECK);
			}
			break;


		//	-------------------------	INIT CONFIGURATION	-------------------------	//


		case ethState_t::ETH_CONFIG_WRITE:
			m_socketTransferDone = false;

			switch(m_currentConfigStat){
				case configState_t::CONFIG_IP:
					Eth::writeBuffer(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_ip, 4, &m_socketTransferDone);
					break;

				case configState_t::CONFIG_GATEWAY:
					Eth::writeBuffer(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_gateway, 4, &m_socketTransferDone);
					break;

				case configState_t::CONFIG_SUBNET:
					Eth::writeBuffer(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_subnet, 4, &m_socketTransferDone);
					break;

				case configState_t::CONFIG_MAC:
					Eth::writeBuffer(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_mac, 6, &m_socketTransferDone);
					break;

				case configState_t::CONFIG_RX_BUFFER_SIZE:
					Eth::writeByte(registerAddr_t::Sn_RXBUF_SIZE_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, (uint8_t)(m_rxBufferSize / 1024), &m_socketTransferDone);
					break;

				case configState_t::CONFIG_TX_BUFFER_SIZE:
					Eth::writeByte(registerAddr_t::Sn_TXBUF_SIZE_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, (uint8_t)(m_txBufferSize / 1024), &m_socketTransferDone);
					break;

				default:
					//	ERROR
					break;
			}

			m_ethState = ethState_t::ETH_CONFIG_WAIT_WRITE;

			break;

		case ethState_t::ETH_CONFIG_WAIT_WRITE:
			if(m_socketTransferDone){
				m_ethState = ethState_t::ETH_CONFIG_NEXT;
			}
			break;

		case ethState_t::ETH_CONFIG_NEXT:
			switch(m_currentConfigStat){
				case configState_t::CONFIG_IP:
					m_currentConfigStat = configState_t::CONFIG_GATEWAY;
					m_ethState = ethState_t::ETH_CONFIG_WRITE;
					break;

				case configState_t::CONFIG_GATEWAY:
					m_currentConfigStat = configState_t::CONFIG_SUBNET;
					m_ethState = ethState_t::ETH_CONFIG_WRITE;
					break;

				case configState_t::CONFIG_SUBNET:
					m_currentConfigStat = configState_t::CONFIG_MAC;
					m_ethState = ethState_t::ETH_CONFIG_WRITE;
					break;

				case configState_t::CONFIG_MAC:
					m_currentConfigStat = configState_t::CONFIG_RX_BUFFER_SIZE;
					m_ethState = ethState_t::ETH_CONFIG_WRITE;
					break;

				case configState_t::CONFIG_RX_BUFFER_SIZE:
					m_currentConfigStat = configState_t::CONFIG_TX_BUFFER_SIZE;
					m_ethState = ethState_t::ETH_CONFIG_WRITE;
					break;

				case configState_t::CONFIG_TX_BUFFER_SIZE:
					m_currentConfigStat = configState_t::CONFIG_NONE;
					m_ethState = ethState_t::ETH_CONFIG_FINISHED;
					break;

				default:
					//	ERROR
					break;
			}
			break;

		case ethState_t::ETH_CONFIG_FINISHED:
			m_initFinishedFlag = true;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	SOCKET STATUS READ	-------------------------	//


		case ethState_t::ETH_SOCKET_STATUS_READ:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_SR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, &m_socketStatusByte, 1, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_STATUS_WAIT_READ;
			break;

		case ethState_t::ETH_SOCKET_STATUS_WAIT_READ:
			if(m_socketTransferDone){
				m_socketStat = (socketStat_t)m_socketStatusByte;
				m_ethState = m_nextStateAfterStatusRead;
			}
			break;

		case ethState_t::ETH_SOCKET_STATUS_CHECK:
			if(m_socketStat == socketStat_t::SOCK_CLOSE_WAIT){
				m_ethState = ethState_t::ETH_SOCKET_DISCONNECT_WRITE_COMMAND;
			}else{
				m_ethState = ethState_t::ETH_IDLE;
			}
			break;


		//	-------------------------	OPEN SOCKET	 -------------------------	//


		case ethState_t::ETH_SOCKET_OPEN_WRITE_MODE:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_MR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketMode_t::TCP_MODE, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_OPEN_WAIT_WRITE_MODE;
			break;

		case ethState_t::ETH_SOCKET_OPEN_WAIT_WRITE_MODE:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_OPEN_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_OPEN_WRITE_PORT:
			m_socketTransferDone = false;
			Eth::writeBuffer(registerAddr_t::Sn_PORT0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_localPortBuffer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_OPEN_WAIT_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_OPEN_WAIT_WRITE_PORT:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_OPEN_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_OPEN_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::OPEN_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_OPEN_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_OPEN_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone){
				if(!m_timeoutTimer.isRunning())
					m_timeoutTimer.startTimer();	//	5s TIMEOUT
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_OPEN_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_OPEN_CHECK_STATUS:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketStat == socketStat_t::SOCK_INIT){
				m_timeoutTimer.stopTimer();
				m_ethState = ethState_t::ETH_SOCKET_OPEN_FINISHED;
			}else{
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_OPEN_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_OPEN_FINISHED:
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	CONNECT SOCKET	-------------------------	//


		case ethState_t::ETH_SOCKET_CONNECT_WRITE_IP:
			m_socketTransferDone = false;
			Eth::writeBuffer(registerAddr_t::Sn_DIPR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_remoteIPBuffer, 4, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_CONNECT_WAIT_WRITE_IP;
			break;

		case ethState_t::ETH_SOCKET_CONNECT_WAIT_WRITE_IP:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_CONNECT_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_CONNECT_WRITE_PORT:
			m_socketTransferDone = false;
			Eth::writeBuffer(registerAddr_t::Sn_DPORT0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_remotePortBuffer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_CONNECT_WAIT_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_CONNECT_WAIT_WRITE_PORT:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_CONNECT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_CONNECT_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::CONNECT_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_CONNECT_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_CONNECT_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone){
				if(!m_timeoutTimer.isRunning())
					m_timeoutTimer.startTimer();	//	5s TIMEOUT
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_CONNECT_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_CONNECT_CHECK_STATUS:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketStat == socketStat_t::SOCK_ESTABLISHED){
				m_timeoutTimer.stopTimer();
				m_ethState = ethState_t::ETH_SOCKET_CONNECT_FINISHED;
			}else if(m_socketStat == socketStat_t::SOCK_INIT || m_socketStat == socketStat_t::SOCK_SYNSENT){
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_CONNECT_CHECK_STATUS);
			}else if(m_socketStat == socketStat_t::SOCK_CLOSED){
				//	ERROR
				m_ethState = ethState_t::ETH_IDLE;
			}
			break;

		case ethState_t::ETH_SOCKET_CONNECT_FINISHED:
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	SEND SOCKET	 -------------------------	//


		case ethState_t::ETH_SOCKET_SEND_READ_TX_FSR:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_TX_FSR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_txFreeSize, 2, &m_socketTransferDone);
			if(!m_timeoutTimer.isRunning())
				m_timeoutTimer.startTimer();	//	5s TIMEOUT
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_TX_FSR;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_TX_FSR:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketTransferDone){
				if(((m_txFreeSize[0] << 8) | m_txFreeSize[1]) >= m_sendLen){
					m_timeoutTimer.stopTimer();
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_WR;
				}else{
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
				}
			}
			break;

		case ethState_t::ETH_SOCKET_SEND_READ_TX_WR:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_TX_WR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_txWritePointer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_READ_TX_WR;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_READ_TX_WR:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_SEND_WRITE_BUFFER;
			break;

		case ethState_t::ETH_SOCKET_SEND_WRITE_BUFFER:
			m_socketTransferDone = false;
			Eth::writeBuffer(((m_txWritePointer[0] << 8) | m_txWritePointer[1]), block_t::SOCKET0_TX_BUFFER_BLOCK, m_sendBuffer, m_sendLen, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_BUFFER;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_BUFFER:
			if(m_socketTransferDone){
				m_nextTxWritePointer = ((m_txWritePointer[0] << 8) | m_txWritePointer[1]) + m_sendLen;
				m_ethState = ethState_t::ETH_SOCKET_SEND_WRITE_TX_WR;
			}
			break;

		case ethState_t::ETH_SOCKET_SEND_WRITE_TX_WR:
			m_socketTransferDone = false;
			m_txWritePointer[0] = (m_nextTxWritePointer >> 8);
			m_txWritePointer[1] = (m_nextTxWritePointer & 0xFF);
			Eth::writeBuffer(registerAddr_t::Sn_TX_WR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_txWritePointer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_TX_WR;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_TX_WR:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_SEND_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_SEND_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::SEND_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_SEND_READ_INTERRUPT_STAT;
			break;

		case ethState_t::ETH_SOCKET_SEND_READ_INTERRUPT_STAT:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_IR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, &m_socketInterruptStat, 1, &m_socketTransferDone);
			if(!m_timeoutTimer.isRunning())
				m_timeoutTimer.startTimer();	//	5s TIMEOUT
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_READ_INTERRUPT_STAT;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_READ_INTERRUPT_STAT:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketTransferDone){
				if(m_socketInterruptStat & socketInterruptStat_t::SEND_OK_INT){
					m_timeoutTimer.stopTimer();
					m_ethState = ethState_t::ETH_SOCKET_SEND_CLEAR_INTERRUPT;
				}else{
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_INTERRUPT_STAT;
				}
			}
			break;

		case ethState_t::ETH_SOCKET_SEND_CLEAR_INTERRUPT:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_IR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketInterruptStat_t::SEND_OK_INT, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_CLEAR_INTERRUPT;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_CLEAR_INTERRUPT:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_SEND_FINISHED;
			break;

		case ethState_t::ETH_SOCKET_SEND_FINISHED:
			m_socketTransferDone = false;
			m_sendFinishedFlag = true;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	RECEIVE SOCKET	 -------------------------	//


		case ethState_t::ETH_SOCKET_RCV_READ_RX_RSR:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_RX_RSR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_rxReceivedSize, 2, &m_socketTransferDone);
			if(!m_timeoutTimer.isRunning())
				m_timeoutTimer.startTimer();	//	5s TIMEOUT
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RSR;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RSR:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketTransferDone){
				m_actualRcvLen = ((m_rxReceivedSize[0] << 8) | m_rxReceivedSize[1]);
				if(m_actualRcvLen == 0){
					m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;	//	No Data was Received Yet
				}else{
					m_timeoutTimer.stopTimer();
					if(m_actualRcvLen > m_usrAskedRcvLen){
						m_rxReceivedSize[0] = (m_usrAskedRcvLen >> 8);		//	User asked less data than received from server
						m_rxReceivedSize[1] = (m_usrAskedRcvLen & 0xFF);
					}
					m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RD;
				}
			}
			break;

		case ethState_t::ETH_SOCKET_RCV_READ_RX_RD:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_RX_RD0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_rxReadPointer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RD;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RD:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_RCV_READ_BUFFER;
			break;

		case ethState_t::ETH_SOCKET_RCV_READ_BUFFER:
			m_socketTransferDone = false;
			Eth::readBuffer(((m_rxReadPointer[0] << 8) | m_rxReadPointer[1]), block_t::SOCKET0_RX_BUFFER_BLOCK, m_rcvBuffer, ((m_rxReceivedSize[0] << 8) | m_rxReceivedSize[1]), &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_READ_BUFFER;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_READ_BUFFER:
			if(m_socketTransferDone){
				m_nextRxReadPointer = (((m_rxReadPointer[0] << 8) | m_rxReadPointer[1]) + ((m_rxReceivedSize[0] << 8) | m_rxReceivedSize[1]));
				m_ethState = ethState_t::ETH_SOCKET_RCV_WRITE_RX_RD;
			}
			break;

		case ethState_t::ETH_SOCKET_RCV_WRITE_RX_RD:
			m_socketTransferDone = false;

			m_rxReadPointer[0] = (m_nextRxReadPointer >> 8);
			m_rxReadPointer[1] = (m_nextRxReadPointer & 0xFF);

			Eth::writeBuffer(registerAddr_t::Sn_RX_RD0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_rxReadPointer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_WRITE_RX_RD;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_WRITE_RX_RD:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_RCV_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_RCV_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::RECV_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_RCV_CLEAR_INTERRUPT;
			break;

		case ethState_t::ETH_SOCKET_RCV_CLEAR_INTERRUPT:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_IR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketInterruptStat_t::RECV_INT, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_CLEAR_INTERRUPT;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_CLEAR_INTERRUPT:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_RCV_FINISHED;
			break;

		case ethState_t::ETH_SOCKET_RCV_FINISHED:
			m_socketTransferDone = false;
			m_rcvFinishedFlag = true;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	DISCONNECT SOCKET	 -------------------------	//


		case ethState_t::ETH_SOCKET_DISCONNECT_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::DISCONNECT_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_DISCONNECT_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_DISCONNECT_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone){
				if(!m_timeoutTimer.isRunning())
					m_timeoutTimer.startTimer();	//	5s TIMEOUT
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_DISCONNECT_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_DISCONNECT_CHECK_STATUS:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketStat == socketStat_t::SOCK_CLOSED){
				m_timeoutTimer.stopTimer();
				m_ethState = ethState_t::ETH_SOCKET_DISCONNECT_FINISHED;
			}else{
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_DISCONNECT_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_DISCONNECT_FINISHED:
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	CLOSE SOCKET	 -------------------------	//


		case ethState_t::ETH_SOCKET_CLOSE_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::CLOSE_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_CLOSE_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_CLOSE_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone){
				if(!m_timeoutTimer.isRunning())
					m_timeoutTimer.startTimer();	//	5s TIMEOUT
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_CLOSE_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_CLOSE_CHECK_STATUS:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketStat == socketStat_t::SOCK_CLOSED){
				m_timeoutTimer.stopTimer();
				m_ethState = ethState_t::ETH_SOCKET_CLOSE_FINISHED;
			}else{
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_CLOSE_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_CLOSE_FINISHED:
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;

		default:
			// ERROR
			break;
	}
}

Eth::~Eth(){}
