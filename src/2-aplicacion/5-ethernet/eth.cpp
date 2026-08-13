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
	m_socketTransferDone = false;
	m_usrDoneFlag = nullptr;
	m_initFinishedFlag = false;
	m_destinationSetFlag = false;
	m_rcvFinishedFlag = false;
	m_rcvBuffer = nullptr;
	m_usrAskedRcvLen = m_actualRcvLen = 0;
	m_sendFinishedFlag = false;
	m_sendLen = m_sendRemainingLen = m_sendProcessedLen = m_sendCurrentLen = 0;
	m_transferInProgressFlag = m_transferBlockDoneFlag = false;
	m_transferRemainingLen = m_transferProcessedLen = m_transferAddr = 0;
	m_transferData = nullptr;
	m_transferContext = transferContext_t::NONE;
	m_usrBufferData = nullptr;
	m_usrBufferRemainingLen = 0;
	m_w5500BufferCurrentAddr = m_w5500BufferCurrentLen = 0;
	m_w5500WrapAroundFlag = false;
	m_w5500BufferSize = m_w5500BufferMask = m_rxBufferSize = m_rxBufferMask = m_txBufferSize = m_txBufferMask = 0;
	m_sendBuffer = nullptr;
	m_currentConfigStat = configState_t::CONFIG_NONE;
	m_socketStat = socketStat_t::SOCK_CLOSED;
	m_nextStateAfterStatusRead = ethState_t::ETH_IDLE;
	m_ethState = ethState_t::ETH_IDLE;
	m_ethError = ethErrorStat_t::ERROR_NONE;
}

void Eth::transferBlock(uint16_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode){
	if((len == 0) || (len > MAX_SPI_TRANSFER_LEN))
		return;

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

void Eth::transfer(uint16_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode){
	if(len == 0)
		return;

	m_transferAddr = addr;
	m_transferRemainingLen = len;
	m_transferProcessedLen = 0;
	m_transferData = data;
	m_transferBlock = block;
	m_transferRWMode = rwMode;
	m_transferOpMode = opMode;

	m_transferBlockDoneFlag = false;
	if(f_done != nullptr)	m_usrDoneFlag = f_done;				//	True when all packets where send/received (app sets it true)
	m_transferInProgressFlag = true;

	uint16_t blockLen = ((m_transferRemainingLen > MAX_SPI_TRANSFER_LEN) ? MAX_SPI_TRANSFER_LEN : m_transferRemainingLen);

	Eth::transferBlock(m_transferAddr, m_transferBlock, m_transferRWMode, m_transferData, blockLen, &m_transferBlockDoneFlag, m_transferOpMode);
}

void Eth::startNextBufferTransfer(){
	if(m_usrBufferRemainingLen == 0)
		return;

	if(m_w5500WrapAroundFlag){	//	Need of W5500 Circular Buffer
		uint16_t offset = (m_w5500BufferCurrentAddr & m_w5500BufferMask);
		uint16_t bytesTillWrap = (m_w5500BufferSize - offset);
		m_w5500BufferCurrentLen = ((m_usrBufferRemainingLen < bytesTillWrap) ? m_usrBufferRemainingLen : bytesTillWrap);
	}else{	//	No need of W5500 Circular Buffer
		m_w5500BufferCurrentLen = m_usrBufferRemainingLen;
	}

	if(m_transferContext == transferContext_t::WRITE_BUFFER){
		Eth::transfer(m_w5500BufferCurrentAddr, m_transferBlock, rwMode_t::WRITE, m_usrBufferData, m_w5500BufferCurrentLen, nullptr, m_transferOpMode);
	}else if(m_transferContext == transferContext_t::READ_BUFFER){
		Eth::transfer(m_w5500BufferCurrentAddr, m_transferBlock, rwMode_t::READ, nullptr, m_w5500BufferCurrentLen, nullptr, m_transferOpMode);
	}
}

void Eth::readByte(uint16_t addr, block_t block, volatile bool *f_done, opMode_t opMode){
	m_transferContext = transferContext_t::GENERIC;
	m_transferByte = 0;
	m_transferBlock = block;
	m_transferRWMode = rwMode_t::READ;
	m_transferOpMode = opMode;
	Eth::transfer(addr, block, rwMode_t::READ, &m_transferByte, 1, f_done, opMode);
}

uint8_t Eth::readByteAndWait(uint16_t addr, block_t block, opMode_t opMode){
	volatile bool f_done = false;

	m_transferBlock = block;
	m_transferRWMode = rwMode_t::READ;
	m_transferOpMode = opMode;

	Eth::transfer(addr, block, rwMode_t::READ, nullptr, 1, &f_done, opMode);

	while(!m_transferBlockDoneFlag);

	return m_rxBuffer[3];
}

void Eth::writeByte(uint16_t addr, block_t block, uint8_t data, volatile bool *f_done, opMode_t opMode){
	m_transferContext = transferContext_t::GENERIC;
	m_transferByte = data;
	m_transferBlock = block;
	m_transferRWMode = rwMode_t::WRITE;
	m_transferOpMode = opMode;
	Eth::transfer(addr, block, rwMode_t::WRITE, &m_transferByte, 1, f_done, opMode);
}

void Eth::readBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode){
	if((len == 0) || (buffer == nullptr))
		return;


	m_transferContext = transferContext_t::READ_BUFFER;

	m_usrBufferData = buffer;
	m_usrBufferRemainingLen = len;
	m_w5500BufferCurrentAddr = addr;

	if(block == block_t::SOCKET0_RX_BUFFER_BLOCK){	//	Need of W5500 circular buffer
		m_w5500WrapAroundFlag = true;
		m_w5500BufferSize = m_rxBufferSize;
		m_w5500BufferMask = m_rxBufferMask;
	}else{	//	No need of W5500 circular buffer
		m_w5500WrapAroundFlag = false;
		m_w5500BufferSize = 0;
		m_w5500BufferMask = 0;
	}

	m_usrDoneFlag = f_done;
	if(m_usrDoneFlag != nullptr)
		*(m_usrDoneFlag) = false;

	m_transferBlock = block;
	m_transferRWMode = rwMode_t::READ;
	m_transferOpMode = opMode;

	//	handler() manages transfer() function so we dont depend on whether or not theres a wrap-around / many SPI packets
	Eth::startNextBufferTransfer();
}

bool Eth::readBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode){
	static volatile bool f_done = false;
	static bool f_first = true;

	if(f_first){
		f_done = false;
		m_transferBlock = block;
		m_transferRWMode = rwMode_t::READ;
		m_transferOpMode = opMode;

		readBuffer(addr, block, buffer, len, &f_done, opMode);
		f_first = false;
	}

	if(f_done){
		f_first = true;
		return true;
	}
	return false;
}

void Eth::writeBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode){
	if((len == 0) || (buffer == nullptr))
		return;

	m_transferContext = transferContext_t::WRITE_BUFFER;

	m_usrBufferData = buffer;
	m_usrBufferRemainingLen = len;
	m_w5500BufferCurrentAddr = addr;

	if(block == block_t::SOCKET0_TX_BUFFER_BLOCK){	//	Need of W5500 circular buffer
		m_w5500WrapAroundFlag = true;
		m_w5500BufferSize = m_txBufferSize;
		m_w5500BufferMask = m_txBufferMask;
	}else{	//	No need of W5500 circular buffer
		m_w5500WrapAroundFlag = false;
		m_w5500BufferSize = 0;
		m_w5500BufferMask = 0;
	}

	m_usrDoneFlag = f_done;
	if(m_usrDoneFlag != nullptr)
		*(m_usrDoneFlag) = false;

	m_transferBlock = block;
	m_transferRWMode = rwMode_t::WRITE;
	m_transferOpMode = opMode;

	//	handler() manages transfer() function so we dont depend on whether or not theres a wrap-around / many SPI packets
	Eth::startNextBufferTransfer();
}

void Eth::writeBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode){
	volatile bool f_done = false;

	m_transferBlock = block;
	m_transferRWMode = rwMode_t::WRITE;
	m_transferOpMode = opMode;

	Eth::writeBuffer(addr, block, buffer, len, &f_done, opMode);

	while(!f_done);
}

void Eth::setIP(uint8_t ip[4], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, ip, 4, f_done, opMode);
}

void Eth::setIPAndWait(uint8_t ip[4], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, ip, 4, opMode);
}

bool Eth::readIP(uint8_t ip[4], opMode_t opMode){
	return Eth::readBufferAndWait(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, ip, 4, opMode);
}

void Eth::setGateway(uint8_t gateway[4], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, gateway, 4, f_done, opMode);
}

void Eth::setGatewayAndWait(uint8_t gateway[4], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, gateway, 4, opMode);
}

bool Eth::readGateway(uint8_t gateway[4], opMode_t opMode){
	return Eth::readBufferAndWait(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, gateway, 4, opMode);
}

void Eth::setSubnet(uint8_t subnet[4], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, subnet, 4, f_done, opMode);
}

void Eth::setSubnetAndWait(uint8_t subnet[4], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, subnet, 4, opMode);
}

bool Eth::readSubnet(uint8_t subnet[4], opMode_t opMode){
	return Eth::readBufferAndWait(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, subnet, 4, opMode);
}

void Eth::setMAC(uint8_t mac[6], volatile bool *f_done, opMode_t opMode){
	Eth::writeBuffer(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, mac, 6, f_done, opMode);
}

void Eth::setMACAndWait(uint8_t mac[6], opMode_t opMode){
	Eth::writeBufferAndWait(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, mac, 6, opMode);
}

bool Eth::readMAC(uint8_t mac[6], opMode_t opMode){
	return Eth::readBufferAndWait(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, mac, 6, opMode);
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
	return ((m_ethState != ethState_t::ETH_IDLE) || m_transferInProgressFlag);
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

bool Eth::isReady() const{
	return (!Eth::isBusy() && m_initFinishedFlag);
}

Eth::ethErrorStat_t Eth::currentError() const{ return m_ethError; }

void Eth::timeoutError(){
	m_timeoutTimer.stopTimer();
	m_ethError = ethErrorStat_t::ERROR_TIMEOUT;
	m_ethState = ethState_t::ETH_IDLE;
}

/*Eth::socketStat_t Eth::socketStatus(){
	return (socketStat_t)Eth::readByteAndWait(registerAddr_t::Sn_SR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK);
}
*/

void Eth::socketOpen(socketMode_t sockMode, uint16_t localPort){
	if(!Eth::isReady())
		return;

	m_localPortBuffer[0] = (localPort >> 8);
	m_localPortBuffer[1] = (localPort & 0xFF);

	m_socketMode = sockMode;

	m_ethState = ethState_t::ETH_SOCKET_OPEN_WRITE_MODE;
}

bool Eth::socketOpened() const{
	if(m_socketMode == socketMode_t::TCP_MODE)
		return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_INIT));
	else if(m_socketMode == socketMode_t::UDP_MODE)
		return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_UDP));
	else
		return false;
}

void Eth::socketConnect(uint8_t remoteIP[4], uint16_t remotePort){
	if(!Eth::isReady())
		return;

	for(uint8_t i = 0; i < 4; i++)
		m_remoteIPBuffer[i] = remoteIP[i];

	m_remotePortBuffer[0] = (remotePort >> 8);
	m_remotePortBuffer[1] = (remotePort & 0xFF);

	m_ethState = ethState_t::ETH_SOCKET_WRITE_IP;
}

bool Eth::socketConnected() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_ESTABLISHED));
}

void Eth::socketSetDestUDP(uint8_t remoteIP[4], uint16_t remotePort){
	if((!Eth::isReady()) || (m_socketMode != socketMode_t::UDP_MODE))
		return;

	m_destinationSetFlag = false;

	for(uint8_t i = 0; i < 4; i++)
		m_remoteIPBuffer[i] = remoteIP[i];

	m_remotePortBuffer[0] = (remotePort >> 8);
	m_remotePortBuffer[1] = (remotePort & 0xFF);

	m_ethState = ethState_t::ETH_SOCKET_WRITE_IP;
}

bool Eth::socketDestSetUDP() const{
	return ((!Eth::isBusy()) && m_destinationSetFlag);
}

void Eth::socketSendTCP(uint8_t *buffer, uint16_t len){
	if((!Eth::isReady()) || (buffer == nullptr) || (len == 0) || (m_socketMode != socketMode_t::TCP_MODE))
		return;

	m_sendBuffer = buffer;
	m_sendLen = len;
	m_sendRemainingLen = m_sendLen;
	m_sendProcessedLen = 0;
	m_sendCurrentLen = 0;
	m_sendFinishedFlag = false;

	m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
}

bool Eth::socketSendFinished() const{ return (!Eth::isBusy() && m_sendFinishedFlag); }

void Eth::socketSendUDP(uint8_t *buffer, uint16_t len){
	if((!Eth::isReady()) 	  ||
	   (buffer == nullptr)    ||
	   (len == 0) 			  ||
	   (len > m_txBufferSize) ||
	   !m_destinationSetFlag  ||
	   (m_socketMode != socketMode_t::UDP_MODE))
		return;

	m_sendBuffer = buffer;
	m_sendLen = len;
	m_sendProcessedLen = 0;
	m_sendCurrentLen = 0;
	m_sendRemainingLen = 0;
	m_sendFinishedFlag = false;

	m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
}

void Eth::socketReceive(uint8_t *buffer, uint16_t maxLen){
	if((!Eth::isReady()) || (buffer == nullptr) || (maxLen == 0))
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

	//	Finished a single SPI transfer (single packet)

	if(m_transferInProgressFlag && m_transferBlockDoneFlag){
		m_transferBlockDoneFlag = false;

		uint16_t blockLen = ((m_transferRemainingLen > MAX_SPI_TRANSFER_LEN) ? MAX_SPI_TRANSFER_LEN : m_transferRemainingLen);

		if(m_transferRWMode == rwMode_t::READ){	//	If it was READ transfer
			if(m_transferContext == transferContext_t::READ_BUFFER){	//	READ BUFFER transfer
				for(uint16_t i = 0; i < blockLen; i++){	//	If m_transferProcessedLen != 0 => bytes transfer > MAX_SPI_TRANSFER_LEN
					m_usrBufferData[(m_transferProcessedLen + i)] = m_rxBuffer[(i + 3)];
				}
			}else if(m_transferContext == transferContext_t::GENERIC){
				m_transferByte = m_rxBuffer[3];
			}
		}
		m_transferProcessedLen += blockLen;
		m_transferRemainingLen -= blockLen;

		if(m_transferRemainingLen > 0){	//	More SPI packets needed for current W5500 transfer
			uint16_t nextBlockLen = ((m_transferRemainingLen > MAX_SPI_TRANSFER_LEN) ? MAX_SPI_TRANSFER_LEN : m_transferRemainingLen);

			if(m_transferRWMode == rwMode_t::WRITE){	//	If it was WRITE transfer
				m_transferData += blockLen;
			}else{	//	If it was READ transfer
				m_transferData = nullptr;
			}

			Eth::transferBlock((m_transferAddr + m_transferProcessedLen), m_transferBlock, m_transferRWMode, m_transferData, nextBlockLen, &m_transferBlockDoneFlag, m_transferOpMode);
		}else{	//	No more SPI packets needed, current transfer finished
			m_transferInProgressFlag = false;
		}
	}

	//	Current ONE byte operation finished ( READ/WRITE BYTE )

	if((!m_transferInProgressFlag) && (m_transferContext == transferContext_t::GENERIC)){
		m_transferContext = transferContext_t::NONE;

		m_transferData = nullptr;
		m_transferRemainingLen = 0;
		m_transferProcessedLen = 0;

		if(m_usrDoneFlag != nullptr){
			*(m_usrDoneFlag) = true;
			m_usrDoneFlag = nullptr;
		}
	}


	//	Current W5500/user-buffer operation finished ( READ/WRITE BUFFER )

	if((!m_transferInProgressFlag) && ((m_transferContext == transferContext_t::READ_BUFFER) || (m_transferContext == transferContext_t::WRITE_BUFFER))){
		m_usrBufferData += m_w5500BufferCurrentLen;
		m_usrBufferRemainingLen -= m_w5500BufferCurrentLen;

		if(m_w5500WrapAroundFlag){	//	W5500 Circular Buffer
			uint16_t offset = (m_w5500BufferCurrentAddr & m_w5500BufferMask);
			offset += m_w5500BufferCurrentLen;

			if(offset >= m_w5500BufferSize)
				offset -= m_w5500BufferSize;

			m_w5500BufferCurrentAddr = offset;
		}else{	//	No need of W5500 Circular Buffer
			m_w5500BufferCurrentAddr += m_w5500BufferCurrentLen;
		}

		if(m_usrBufferRemainingLen == 0){	//	Completed User Operation
			m_transferContext = transferContext_t::NONE;

			m_transferData = nullptr;
			m_transferRemainingLen = 0;
			m_transferProcessedLen = 0;

			m_usrBufferData = nullptr;
			m_w5500BufferCurrentLen = 0;

			if(m_usrDoneFlag != nullptr){
				*(m_usrDoneFlag) = true;
				m_usrDoneFlag = nullptr;
			}
		}else{	//	More data remains
			Eth::startNextBufferTransfer();
		}
	}


	// Ethernet States Machine


	switch(m_ethState){
		case ethState_t::ETH_IDLE:
			if(m_socketCloseMode == socketCloseMode_t::AUTO_CLOSE && !m_transferInProgressFlag){
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
			Eth::writeByte(registerAddr_t::Sn_MR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_socketMode, &m_socketTransferDone);
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
			if((m_socketMode == socketMode_t::TCP_MODE) && (m_socketStat == socketStat_t::SOCK_INIT)){	//	TCP MODE
				m_timeoutTimer.stopTimer();
				m_ethState = ethState_t::ETH_SOCKET_OPEN_FINISHED;
			}else if((m_socketMode == socketMode_t::UDP_MODE) && (m_socketStat == socketStat_t::SOCK_UDP)){	//	UDP MODE
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


		//	-------------------------	CONNECT SOCKET (TCP)	-------------------------	//
		//	-------------------------  SET DESTINATION  (UDP)	-------------------------	//


		case ethState_t::ETH_SOCKET_WRITE_IP:
			m_socketTransferDone = false;
			Eth::writeBuffer(registerAddr_t::Sn_DIPR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_remoteIPBuffer, 4, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_WAIT_WRITE_IP;
			break;

		case ethState_t::ETH_SOCKET_WAIT_WRITE_IP:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_WRITE_PORT:
			m_socketTransferDone = false;
			Eth::writeBuffer(registerAddr_t::Sn_DPORT0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_remotePortBuffer, 2, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_WAIT_WRITE_PORT;
			break;

		case ethState_t::ETH_SOCKET_WAIT_WRITE_PORT:
			if(m_socketTransferDone){
				if(m_socketMode == socketMode_t::TCP_MODE){
					m_ethState = ethState_t::ETH_SOCKET_CONNECT_WRITE_COMMAND;
				}else if(m_socketMode == socketMode_t::UDP_MODE){
					m_destinationSetFlag = true;
					m_ethState = ethState_t::ETH_SOCKET_CONNECT_FINISHED;
				}
			}
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
				m_timeoutTimer.stopTimer();
				m_ethError = ethErrorStat_t::ERROR_SOCK_CLOSED;
				m_ethState = ethState_t::ETH_IDLE;
			}
			break;

		case ethState_t::ETH_SOCKET_CONNECT_FINISHED:
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	SEND SOCKET (TCP & UDP)	 -------------------------	//


		case ethState_t::ETH_SOCKET_SEND_READ_TX_FSR:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_TX_FSR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_txFreeSize, 2, &m_socketTransferDone);
			if(!m_timeoutTimer.isRunning())
				m_timeoutTimer.startTimer();	//	5s TIMEOUT
			if(m_socketMode == socketMode_t::TCP_MODE)
				m_ethState = ethState_t::ETH_SOCKET_SEND_TCP_WAIT_TX_FSR;
			else if(m_socketMode == socketMode_t::UDP_MODE)
				m_ethState = ethState_t::ETH_SOCKET_SEND_UDP_WAIT_TX_FSR;
			break;


		//	-------------------------	SEND SOCKET (TCP)	 -------------------------	//


		case ethState_t::ETH_SOCKET_SEND_TCP_WAIT_TX_FSR:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketTransferDone){
				uint16_t availableSendLen = ((m_txFreeSize[0] << 8) | m_txFreeSize[1]);
				m_sendCurrentLen = ((availableSendLen >= m_sendRemainingLen) ? m_sendRemainingLen : availableSendLen);

				if(m_sendCurrentLen != 0){
					m_timeoutTimer.stopTimer();
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_WR;
				}else{
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
				}
			}
			break;


		//	-------------------------	SEND SOCKET (UDP)	 -------------------------	//


		case ethState_t::ETH_SOCKET_SEND_UDP_WAIT_TX_FSR:
			if(m_timeoutTimer.singleTimerExpired()){
				Eth::timeoutError();
				break;
			}
			if(m_socketTransferDone){
				uint16_t availableSendLen = ((m_txFreeSize[0] << 8) | m_txFreeSize[1]);

				if(availableSendLen >= m_sendLen){
					m_timeoutTimer.stopTimer();
					m_sendCurrentLen = m_sendLen;
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_WR;
				}else{	//	NO SPACE FOR UDP TRANSFER
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
				}
			}
			break;


		//	-------------------------	SEND SOCKET (TCP & UDP)	 -------------------------	//


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
			Eth::writeBuffer(((m_txWritePointer[0] << 8) | m_txWritePointer[1]), block_t::SOCKET0_TX_BUFFER_BLOCK, (m_sendBuffer + m_sendProcessedLen), m_sendCurrentLen, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_BUFFER;
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_WRITE_BUFFER:
			if(m_socketTransferDone){
				if(m_socketMode == socketMode_t::TCP_MODE){
					m_sendRemainingLen -= m_sendCurrentLen;
					m_sendProcessedLen += m_sendCurrentLen;
				}
				m_nextTxWritePointer = ((m_txWritePointer[0] << 8) | m_txWritePointer[1]) + m_sendCurrentLen;
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
				}else if(m_socketInterruptStat & socketInterruptStat_t::TIMEOUT_INT){
					Eth::timeoutError();
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
			if(m_socketTransferDone){
				if(m_sendRemainingLen == 0){
					m_ethState = ethState_t::ETH_SOCKET_SEND_FINISHED;
				}else{
					m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
				}
			}
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
