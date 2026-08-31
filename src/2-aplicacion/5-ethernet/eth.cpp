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
	m_dhcpState = dhcpState_t::DHCP_IDLE;
	m_dhcpFinishedFlag = false;
	m_dnsFinishedFlag = false;
	m_dnsInProgressFlag = false;
	m_dnsParseState = dnsParseState_t::DNS_PARSE_NONE;
	m_httpState = httpState_t::HTTP_IDLE;
	m_httpInProgressFlag = false;
	m_httpFinishedFlag = false;
	m_httpErrorOccurred = false;
	m_httpHeartbeatInProgressFlag = false;
	m_httpHeartbeatFinishedFlag = false;
	m_httpUploading_usrFlag = false;
	m_httpHeartBeating_usrFlag = false;
	m_currentConfigStat = configState_t::CONFIG_NONE;
	m_socketStat = socketStat_t::SOCK_CLOSED;
	m_nextStateAfterStatusRead = ethState_t::ETH_IDLE;
	m_ethState = ethState_t::ETH_IDLE;
	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethStateWhenLastError = ethState_t::ETH_IDLE;
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

	//	stateMachine() manages transfer() function so we dont depend on whether or not theres a wrap-around / many SPI packets
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

	//	stateMachine() manages transfer() function so we dont depend on whether or not theres a wrap-around / many SPI packets
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



void Eth::init(uint8_t mac[6], socketBufferSize_t rxBufferSize,
		  	   socketBufferSize_t txBufferSize, socketCloseMode_t closeMode,
			   opMode_t opMode){

	if(Eth::isBusy())
		return;

	m_initConfigMode = initConfigMode_t::INIT_WITH_DHCP;
	m_socketCloseMode = closeMode;
	m_opMode = opMode;

	for(uint8_t i = 0; i < 6; i++)	m_mac[i] = mac[i];

	m_rxBufferSize = (uint16_t)(rxBufferSize * 1024);
	m_rxBufferMask = m_rxBufferSize - 1;

	m_txBufferSize = (uint16_t)(txBufferSize * 1024);
	m_txBufferMask = m_txBufferSize - 1;

	m_initFinishedFlag = false;
	m_dhcpState = dhcpState_t::DHCP_IDLE;
	m_currentConfigStat = configState_t::CONFIG_MAC;
	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_CONFIG_WRITE;
	m_ethStateWhenLastError = ethState_t::ETH_IDLE;
}

void Eth::init(uint8_t ip[4], uint8_t gateway[4], uint8_t subnet[4], uint8_t mac[6],
		  	   socketBufferSize_t rxBufferSize, socketBufferSize_t txBufferSize,
			   socketCloseMode_t closeMode, opMode_t opMode){

	if(Eth::isBusy())
		return;

	m_initConfigMode = initConfigMode_t::INIT_WITH_STATIC_IP;
	m_socketCloseMode = closeMode;
	m_opMode = opMode;

	for(uint8_t i = 0; i < 4; i++){
		m_ip[i] = ip[i];
		m_gateway[i] = gateway[i];
		m_subnet[i] = subnet[i];
		m_dhcpDNS[i] = 0;
	}

	for(uint8_t i = 0; i < 6; i++)	m_mac[i] = mac[i];

	m_rxBufferSize = (uint16_t)(rxBufferSize * 1024);
	m_rxBufferMask = m_rxBufferSize - 1;

	m_txBufferSize = (uint16_t)(txBufferSize * 1024);
	m_txBufferMask = m_txBufferSize - 1;

	m_initFinishedFlag = false;
	m_dhcpState = dhcpState_t::DHCP_IDLE;
	m_currentConfigStat = configState_t::CONFIG_MAC;
	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_CONFIG_WRITE;
	m_ethStateWhenLastError = ethState_t::ETH_IDLE;
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

Eth::ethState_t Eth::stateWhenLastError() const{ return m_ethStateWhenLastError; }

void Eth::timeoutError(){
	m_timeoutTimer.stopTimer();
	m_ethError = ethErrorStat_t::ERROR_TIMEOUT;
	m_ethStateWhenLastError = m_ethState;

	if(m_httpInProgressFlag){
		Eth::HTTPtimeoutError(m_ethStateWhenLastError);
		if(m_ethState == ethState_t::ETH_SOCKET_DISCONNECT_CHECK_STATUS)
			return;	//	If disconnect is required, keep ethState for that
	}

	m_ethState = ethState_t::ETH_IDLE;
}

/*Eth::socketStat_t Eth::socketStatus(){
	return (socketStat_t)Eth::readByteAndWait(registerAddr_t::Sn_SR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK);
}
*/

void Eth::socketOpen(socketMode_t sockMode, uint16_t localPort){
	if(!Eth::isReady())
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_localPortBuffer[0] = (localPort >> 8);
	m_localPortBuffer[1] = (localPort & 0xFF);

	m_socketMode = sockMode;

	m_ethError = ethErrorStat_t::ERROR_NONE;
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

void Eth::socketTCPconnect(uint8_t remoteIP[4], uint16_t remotePort){
	if(!Eth::isReady() || (m_socketMode != socketMode_t::TCP_MODE) || (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	for(uint8_t i = 0; i < 4; i++)
		m_remoteIPBuffer[i] = remoteIP[i];

	m_remotePortBuffer[0] = (remotePort >> 8);
	m_remotePortBuffer[1] = (remotePort & 0xFF);

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_WRITE_IP;
}

void Eth::socketTCPconnect(uint16_t remotePort){
	if(!Eth::DNSresolveFinished())
		return;

	Eth::socketTCPconnect(m_dnsResolvedIP, remotePort);
}

bool Eth::socketTCPconnected() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_ESTABLISHED));
}

void Eth::socketUDPsetDest(uint8_t remoteIP[4], uint16_t remotePort){
	if((!Eth::isReady()) || (m_socketMode != socketMode_t::UDP_MODE) || (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_destinationSetFlag = false;

	for(uint8_t i = 0; i < 4; i++)
		m_remoteIPBuffer[i] = remoteIP[i];

	m_remotePortBuffer[0] = (remotePort >> 8);
	m_remotePortBuffer[1] = (remotePort & 0xFF);

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_WRITE_IP;
}

void Eth::socketUDPsetDest(uint16_t remotePort){
	if(!Eth::DNSresolveFinished())
		return;

	Eth::socketUDPsetDest(m_dnsResolvedIP, remotePort);
}

bool Eth::socketUDPdestSet() const{
	return ((!Eth::isBusy()) && m_destinationSetFlag && !(Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::socketTCPsend(const void *buffer, uint16_t len){
	if((!Eth::isReady())   ||
	   (buffer == nullptr) ||
	   (len == 0) 		   ||
	   (m_socketMode != socketMode_t::TCP_MODE) ||
	   (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_sendBuffer = (uint8_t *)buffer;
	m_sendLen = len;
	m_sendRemainingLen = m_sendLen;
	m_sendProcessedLen = 0;
	m_sendCurrentLen = 0;
	m_sendFinishedFlag = false;

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
}

void Eth::socketUDPsend(uint8_t *buffer, uint16_t len){
	if((!Eth::isReady()) 	  ||
	   (buffer == nullptr)    ||
	   (len == 0) 			  ||
	   (len > m_txBufferSize) ||
	   !m_destinationSetFlag  ||
	   (m_socketMode != socketMode_t::UDP_MODE) ||
	   (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_sendBuffer = buffer;
	m_sendLen = len;
	m_sendProcessedLen = 0;
	m_sendCurrentLen = 0;
	m_sendRemainingLen = 0;
	m_sendFinishedFlag = false;

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
}

bool Eth::socketSendFinished() const{
	return (!Eth::isBusy() && m_sendFinishedFlag && !(Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::socketTCPreceive(const void *buffer, uint16_t maxLen){
	if((!Eth::isReady())   ||
	   (buffer == nullptr) ||
	   (maxLen == 0) 	   ||
	   (m_socketMode != socketMode_t::TCP_MODE) ||
	   (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_rcvBuffer = (uint8_t *)buffer;
	m_usrAskedRcvLen = maxLen;
	m_actualRcvLen = 0;
	m_rcvFinishedFlag = false;

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;
}

void Eth::socketUDPreceive(uint8_t *buffer, uint16_t maxLen){
	if((!Eth::isReady())   		 ||
	   (buffer == nullptr) 		 ||
	   (maxLen == 0) 	   		 ||
	   (maxLen > m_rxBufferSize) ||
	   (m_socketMode != socketMode_t::UDP_MODE) ||
	   (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_rcvBuffer = buffer;
	m_usrAskedRcvLen = maxLen;
	m_actualRcvLen = 0;
	m_udpPayloadLen = 0;
	m_rcvFinishedFlag = false;
	for(uint8_t i = 0; i < Eth::UDP_HEADER_LEN; i++) m_headerUDP[i] = 0;
	for(uint8_t i = 0; i < 4; i++) m_udpRcvRemoteIP[i] = 0;
	for(uint8_t i = 0; i < 2; i++) m_udpRcvRemotePort[i] = 0;

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;
}

uint16_t Eth::socketReceivedLen() const{ return m_actualRcvLen; }

bool Eth::socketReceiveFinished() const{
	return (!Eth::isBusy() && m_rcvFinishedFlag && !(Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::socketTCPdisconnect(){
	if(!Eth::isReady() || (m_socketMode != socketMode_t::TCP_MODE) || (Eth::socketStatus() == socketStat_t::SOCK_CLOSED))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_DISCONNECT_WRITE_COMMAND;
}

bool Eth::socketTCPdisconnectFinished() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::socketClose(){
	if(!Eth::isReady())
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_rcvFinishedFlag = false;
	m_sendFinishedFlag = false;

	if(m_socketMode == socketMode_t::UDP_MODE)
		m_destinationSetFlag = false;

	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_SOCKET_CLOSE_CLEAR_INTERRUPT;
}

bool Eth::socketCloseFinished() const{
	return ((!Eth::isBusy()) && (Eth::socketStatus() == socketStat_t::SOCK_CLOSED));
}

void Eth::DHCPgenerateXid() {
	uint32_t xID = 0;

	//	Generates Random Number as Unique Transaction ID
	xID ^= (m_mac[0] << 24);
	xID ^= (m_mac[1] << 16);
	xID ^= (m_mac[2] << 8);
	xID ^= (m_mac[3] << 0);
	xID ^= (m_mac[4] << 8);
	xID ^= (m_mac[5] << 0);
	xID ^= SysTimer::randomTick;

	m_dhcpTransactionID[0] = (uint8_t)(xID >> 24);
	m_dhcpTransactionID[1] = (uint8_t)(xID >> 16);
	m_dhcpTransactionID[2] = (uint8_t)(xID >> 8);
	m_dhcpTransactionID[3] = (uint8_t)(xID >> 0);
}

void Eth::DHCPstart(){
	if(m_ethState != ethState_t::ETH_DHCP_START)
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_dhcpInProgressFlag = true;
	m_dhcpOptionErrorFlag = false;
	m_dhcpOptionNACKfound = false;
	m_dhcpFinishedFlag = false;
	m_dhcpRxLen = 0;
	m_dhcpTxLen = 0;
	m_dhcpRetryCount = 0;
	m_dhcpLeaseTime = 0;

	Eth::DHCPgenerateXid();

	for(uint8_t i = 0; i < 4; i++){
		m_ip[i] = 0;
		m_subnet[i] = 0;
		m_gateway[i] = 0;
		m_dhcpOfferedIP[i] = 0;
		m_dhcpServerIP[i] = 0;
		m_dhcpDNS[i] = 0;
		m_remoteIPBuffer[i] = 255;
	}

	m_localPortBuffer[0] = 0;
	m_localPortBuffer[1] = Eth::DHCP_CLIENT_PORT;
	m_remotePortBuffer[0] = 0;
	m_remotePortBuffer[1] = Eth::DHCP_SERVER_PORT;

	m_socketMode = socketMode_t::UDP_MODE;
	m_dhcpState = dhcpState_t::DHCP_START;
}

void Eth::DHCPbuildDiscover(){
	uint16_t index = 0;

	//	BOOTP HEADER
	m_dhcpTxBuffer[index++] = Eth::DHCP_OP_BOOT_REQUEST;
	m_dhcpTxBuffer[index++] = Eth::DHCP_HTYPE_ETHERNET;
	m_dhcpTxBuffer[index++] = Eth::DHCP_HLEN_ETHERNET;
	m_dhcpTxBuffer[index++] = 0;	//	Hops

	//	X_ID	transaction id
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[0];
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[1];
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[2];
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[3];

	//	SECONDS ELAPSED
	m_dhcpTxBuffer[index++] = 0;
	m_dhcpTxBuffer[index++] = 0;

	//	BROADCAST FLAGS
	m_dhcpTxBuffer[index++] = 0x80;
	m_dhcpTxBuffer[index++] = 0x00;

	//	CI_ADDR		client ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	YI_ADDR		your ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	SI_ADDR		server ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	GI_ADDR		gateway ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	CHA_ADDR	client hardware address
	for(uint8_t i = 0; i < 6; i++)	m_dhcpTxBuffer[index++] = m_mac[i];

	//	remaining CHA_ADDR
	for(uint8_t i = 0; i < 10; i++)	m_dhcpTxBuffer[index++] = 0;

	//	SNAME		server hostname
	for(uint8_t i = 0; i < 64; i++)	m_dhcpTxBuffer[index++] = 0;

	//	FILE
	for(uint8_t i = 0; i < 128; i++)	m_dhcpTxBuffer[index++] = 0;

	//	DHCP MAGIC COOKIE
	m_dhcpTxBuffer[index++] = 99;
	m_dhcpTxBuffer[index++] = 130;
	m_dhcpTxBuffer[index++] = 83;
	m_dhcpTxBuffer[index++] = 99;

	//	OPTION 53 - DHCP Message Type
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_MESSAGE_TYPE;
	m_dhcpTxBuffer[index++] = 1;
	m_dhcpTxBuffer[index++] = Eth::DHCP_DISCOVER;

	//	OPTION 55 - DHCP Parameter Request List
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_PARAMETER_REQUEST;
	m_dhcpTxBuffer[index++] = 3;
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_SUBNET_MASK;
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_ROUTER;
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_DNS;

	//	OPTION 61 - Client Identifier
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_CLIENT_IDENTIFIER;
	m_dhcpTxBuffer[index++] = 7;
	m_dhcpTxBuffer[index++] = 1;	//	Ethernet
	for(uint8_t i = 0; i < 6; i++)	m_dhcpTxBuffer[index++] = m_mac[i];

	//	END
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_END;

	m_dhcpTxLen = index;

	m_sendBuffer = m_dhcpTxBuffer;
	m_sendLen = m_dhcpTxLen;
	m_sendProcessedLen = 0;
	m_sendCurrentLen = 0;
	m_sendRemainingLen = 0;
	m_sendFinishedFlag = false;

	m_dhcpState = dhcpState_t::DHCP_BUILD_DISCOVER;
}

void Eth::DHCPwaitOffer(){
	m_rcvBuffer = m_dhcpRxBuffer;
	m_usrAskedRcvLen = Eth::DHCP_BUFFER_LEN;
	m_actualRcvLen = 0;
	m_udpPayloadLen = 0;
	m_rcvFinishedFlag = false;
	for(uint8_t i = 0; i < Eth::UDP_HEADER_LEN; i++) m_headerUDP[i] = 0;
	for(uint8_t i = 0; i < 4; i++) m_udpRcvRemoteIP[i] = 0;
	for(uint8_t i = 0; i < 2; i++) m_udpRcvRemotePort[i] = 0;

	m_dhcpState = dhcpState_t::DHCP_WAIT_OFFER;
}

bool Eth::DHCPparseOffer(){
	m_dhcpState = dhcpState_t::DHCP_PARSE_OFFER;

	if(m_dhcpRxLen < (Eth::DHCP_FIXED_HEADER_LEN + Eth::DHCP_MAGIC_COOKIE_LEN))
		return false;

	m_dhcpCurrentRxIndex = 0;

	//	OP
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != Eth::DHCP_OP_BOOT_REPLY)
		return false;

	//	HTYPE
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != Eth::DHCP_HTYPE_ETHERNET)
		return false;

	//	HLEN
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != Eth::DHCP_HLEN_ETHERNET)
		return false;

	//	HOPS
	m_dhcpCurrentRxIndex++;

	//	XID
	for(uint8_t i = 0; i < 4; i++){
		if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != m_dhcpTransactionID[i])
			return false;
	}

	//	SECS
	m_dhcpCurrentRxIndex += 2;

	//	FLAGS
	m_dhcpCurrentRxIndex += 2;

	//	CIADDR
	m_dhcpCurrentRxIndex += 4;

	//	YIADDR
	for(uint8_t i = 0; i < 4; i++)
		m_dhcpOfferedIP[i] = m_dhcpRxBuffer[m_dhcpCurrentRxIndex++];	//	This gonna be our m_ip

	//	SIADDR
	m_dhcpCurrentRxIndex += 4;

	//	GIADDR
	m_dhcpCurrentRxIndex += 4;

	//	CHADDR
	for(uint8_t i = 0; i < 6; i++){
		if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != m_mac[i])
			return false;
	}

	//	REMAINING CHADDR
	m_dhcpCurrentRxIndex += 10;

	//	SNAME
	m_dhcpCurrentRxIndex += 64;

	//	FILE
	m_dhcpCurrentRxIndex += 128;

	//	MAGIC COOKIE
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 99)
		return false;

	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 130)
		return false;

	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 83)
		return false;

	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 99)
		return false;

	m_dhcpMsgTypeFound_flag = false;
	m_dhcpServerIpFound_flag = false;
	m_dhcpSubnetFound_flag = false;
	m_dhcpGatewayFound_flag = false;
	m_dhcpDnsFound_flag = false;

	m_dhcpOptionErrorFlag = false;

	return true;
}

void Eth::DHCPvalidateOption(uint8_t expectedMsgType, ethState_t nextStateIfOptionOK){
	if(m_dhcpCurrentRxIndex >= m_dhcpRxLen){
		m_ethState = nextStateIfOptionOK;
		return;
	}

	//	DHCP OPTIONS

/*	+--------+--------+-------------------+
	|  CODE  | LENGTH |       DATA        |
	+--------+--------+-------------------+
	  1 byte   1 byte      LENGTH bytes
*/

	uint8_t option = m_dhcpRxBuffer[m_dhcpCurrentRxIndex++];

	if(option == Eth::DHCP_OPTION_PAD)
		return;	//	Nothing To do

	if(option == Eth::DHCP_OPTION_END){
		m_ethState = nextStateIfOptionOK;
		return;
	}

	if(m_dhcpCurrentRxIndex >= m_dhcpRxLen){	//	Need more length
		m_dhcpOptionErrorFlag = true;
		return;
	}

	uint8_t optionLen = m_dhcpRxBuffer[m_dhcpCurrentRxIndex++];

	if((m_dhcpCurrentRxIndex + optionLen) > m_dhcpRxLen){
		m_dhcpOptionErrorFlag = true;
		return;
	}

	bool f_valid = true;

	switch(option){
		case Eth::DHCP_OPTION_MESSAGE_TYPE:
			if(optionLen != 1){
				m_dhcpOptionErrorFlag = true;
				f_valid = false;
			}else if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex] == Eth::DHCP_NACK){
				m_dhcpOptionNACKfound = true;
			}else if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex] == expectedMsgType){
				m_dhcpMsgTypeFound_flag = true;
			}else{
				m_dhcpOptionErrorFlag = true;
				f_valid = false;
			}
			break;

		case Eth::DHCP_OPTION_SERVER_IDENTIFIER:
			if(optionLen == 4){
				for(uint8_t i = 0; i < 4; i++)
					m_dhcpServerIP[i] = m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + i)];

				m_dhcpServerIpFound_flag = true;
			}else{
				m_dhcpOptionErrorFlag = true;
				f_valid = false;
			}
			break;

		case Eth::DHCP_OPTION_SUBNET_MASK:
			if(optionLen == 4){
				for(uint8_t i = 0; i < 4; i++)
					m_subnet[i] = m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + i)];	//	If offered, this is our subnet

				m_dhcpSubnetFound_flag = true;
			}else{
				m_dhcpOptionErrorFlag = true;
				f_valid = false;;
			}
			break;

		case Eth::DHCP_OPTION_ROUTER:
			if(optionLen >= 4){
				for(uint8_t i = 0; i < 4; i++)
					m_gateway[i] = m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + i)];	//	If offered, this is our gateway

				m_dhcpGatewayFound_flag = true;
			}else{
				m_dhcpOptionErrorFlag = true;
				f_valid = false;
			}
			break;

		case Eth::DHCP_OPTION_DNS:
			if(optionLen >= 4){
				for(uint8_t i = 0; i < 4; i++)
					m_dhcpDNS[i] = m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + i)];

				m_dhcpDnsFound_flag = true;
			}else{
				m_dhcpOptionErrorFlag = true;
				f_valid = false;
			}
			break;

		case Eth::DHCP_OPTION_LEASE_TIME:
			if(optionLen == 4){
				m_dhcpLeaseTime = ((uint32_t)(m_dhcpRxBuffer[m_dhcpCurrentRxIndex]) << 24) 	     |
								  ((uint32_t)(m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + 1)]) << 16) |
								  ((uint32_t)(m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + 2)]) << 8)  |
								  ((uint32_t)(m_dhcpRxBuffer[(m_dhcpCurrentRxIndex + 3)]) << 0);

				m_dhcpLeaseTimeFound_flag = true;
			}else{
				m_dhcpOptionErrorFlag = true;
				f_valid = false;
			}
			break;

		default:
			//	Not interested in this DHCP option
			break;
	}

	if(f_valid)
		m_dhcpCurrentRxIndex += optionLen;
}

void Eth::DHCPbuildRequest(){
	uint16_t index = 0;

	//	BOOTP HEADER
	m_dhcpTxBuffer[index++] = Eth::DHCP_OP_BOOT_REQUEST;
	m_dhcpTxBuffer[index++] = Eth::DHCP_HTYPE_ETHERNET;
	m_dhcpTxBuffer[index++] = Eth::DHCP_HLEN_ETHERNET;
	m_dhcpTxBuffer[index++] = 0;	//	Hops

	//	X_ID	transaction id
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[0];
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[1];
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[2];
	m_dhcpTxBuffer[index++] = m_dhcpTransactionID[3];

	//	SECONDS ELAPSED
	m_dhcpTxBuffer[index++] = 0;
	m_dhcpTxBuffer[index++] = 0;

	//	BROADCAST FLAGS
	m_dhcpTxBuffer[index++] = 0x80;
	m_dhcpTxBuffer[index++] = 0x00;

	//	CI_ADDR		client ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	YI_ADDR		your ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	SI_ADDR		server ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	GI_ADDR		gateway ip address
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = 0;

	//	CHA_ADDR	client hardware address
	for(uint8_t i = 0; i < 6; i++)	m_dhcpTxBuffer[index++] = m_mac[i];

	//	remaining CHA_ADDR
	for(uint8_t i = 0; i < 10; i++)	m_dhcpTxBuffer[index++] = 0;

	//	SNAME		server hostname
	for(uint8_t i = 0; i < 64; i++)	m_dhcpTxBuffer[index++] = 0;

	//	FILE
	for(uint8_t i = 0; i < 128; i++)	m_dhcpTxBuffer[index++] = 0;

	//	DHCP MAGIC COOKIE
	m_dhcpTxBuffer[index++] = 99;
	m_dhcpTxBuffer[index++] = 130;
	m_dhcpTxBuffer[index++] = 83;
	m_dhcpTxBuffer[index++] = 99;

	//	OPTION 53 - DHCP Message Type
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_MESSAGE_TYPE;
	m_dhcpTxBuffer[index++] = 1;
	m_dhcpTxBuffer[index++] = Eth::DHCP_REQUEST;

	//	OPTION 50 - DHCP Requested IP Address
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_REQUESTED_IP;
	m_dhcpTxBuffer[index++] = 4;
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = m_dhcpOfferedIP[i];

	//	OPTION 54 - DHCP Server Identifier
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_SERVER_IDENTIFIER;
	m_dhcpTxBuffer[index++] = 4;
	for(uint8_t i = 0; i < 4; i++)	m_dhcpTxBuffer[index++] = m_dhcpServerIP[i];

	//	OPTION 55 - DHCP Parameter Request List
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_PARAMETER_REQUEST;
	m_dhcpTxBuffer[index++] = 3;
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_SUBNET_MASK;
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_ROUTER;
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_DNS;

	//	OPTION 61 - Client Identifier
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_CLIENT_IDENTIFIER;
	m_dhcpTxBuffer[index++] = 7;
	m_dhcpTxBuffer[index++] = 1;	//	Ethernet
	for(uint8_t i = 0; i < 6; i++)	m_dhcpTxBuffer[index++] = m_mac[i];

	//	END
	m_dhcpTxBuffer[index++] = Eth::DHCP_OPTION_END;

	m_dhcpTxLen = index;

	m_sendBuffer = m_dhcpTxBuffer;
	m_sendLen = m_dhcpTxLen;
	m_sendProcessedLen = 0;
	m_sendCurrentLen = 0;
	m_sendRemainingLen = 0;
	m_sendFinishedFlag = false;

	m_dhcpState = dhcpState_t::DHCP_BUILD_REQUEST;
}

void Eth::DHCPwaitACK(){
	m_rcvBuffer = m_dhcpRxBuffer;
	m_usrAskedRcvLen = Eth::DHCP_BUFFER_LEN;
	m_actualRcvLen = 0;
	m_udpPayloadLen = 0;
	m_rcvFinishedFlag = false;

	m_dhcpState = dhcpState_t::DHCP_WAIT_ACK;

	for(uint8_t i = 0; i < Eth::UDP_HEADER_LEN; i++) m_headerUDP[i] = 0;
	for(uint8_t i = 0; i < 4; i++) m_udpRcvRemoteIP[i] = 0;
	for(uint8_t i = 0; i < 2; i++) m_udpRcvRemotePort[i] = 0;
}

bool Eth::DHCPparseACK(){
	m_dhcpState = dhcpState_t::DHCP_PARSE_ACK;

	if(m_dhcpRxLen < (Eth::DHCP_FIXED_HEADER_LEN + Eth::DHCP_MAGIC_COOKIE_LEN))
		return false;

	m_dhcpCurrentRxIndex = 0;

	//	OP
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != Eth::DHCP_OP_BOOT_REPLY)
		return false;

	//	HTYPE
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != Eth::DHCP_HTYPE_ETHERNET)
		return false;

	//	HLEN
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != Eth::DHCP_HLEN_ETHERNET)
		return false;

	//	HOPS
	m_dhcpCurrentRxIndex++;

	//	XID
	for(uint8_t i = 0; i < 4; i++){
		if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != m_dhcpTransactionID[i])
			return false;
	}

	//	SECS
	m_dhcpCurrentRxIndex += 2;

	//	FLAGS
	m_dhcpCurrentRxIndex += 2;

	//	CIADDR
	m_dhcpCurrentRxIndex += 4;

	//	YIADDR
	for(uint8_t i = 0; i < 4; i++)
		m_dhcpOfferedIP[i] = m_dhcpRxBuffer[m_dhcpCurrentRxIndex++];

	//	SIADDR
	m_dhcpCurrentRxIndex += 4;

	//	GIADDR
	m_dhcpCurrentRxIndex += 4;

	//	CHADDR
	for(uint8_t i = 0; i < 6; i++){
		if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != m_mac[i])
			return false;
	}

	//	REMAINING CHADDR
	m_dhcpCurrentRxIndex += 10;

	//	SNAME
	m_dhcpCurrentRxIndex += 64;

	//	FILE
	m_dhcpCurrentRxIndex += 128;

	//	MAGIC COOKIE
	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 99)
		return false;

	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 130)
		return false;

	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 83)
		return false;

	if(m_dhcpRxBuffer[m_dhcpCurrentRxIndex++] != 99)
		return false;

	m_dhcpMsgTypeFound_flag = false;
	m_dhcpServerIpFound_flag = false;
	m_dhcpSubnetFound_flag = false;
	m_dhcpGatewayFound_flag = false;
	m_dhcpDnsFound_flag = false;
	m_dhcpLeaseTimeFound_flag = false;

	m_dhcpOptionErrorFlag = false;
	m_dhcpOptionNACKfound = false;

	return true;
}

bool Eth::DHCPfinished() const{ return m_dhcpFinishedFlag; }

void Eth::DNSgenerateXid(){
	m_dnsTransactionID = 0;

	//	Generates Random Number as Unique Transaction ID
	m_dnsTransactionID ^= (m_mac[0] << 8);
	m_dnsTransactionID ^= (m_mac[1] << 0);
	m_dnsTransactionID ^= (m_mac[4] << 8);
	m_dnsTransactionID ^= (m_mac[5] << 0);
	m_dnsTransactionID ^= (m_ip[0] << 8);
	m_dnsTransactionID ^= (m_ip[3] << 0);
	m_dnsTransactionID ^= (SysTimer::randomTick % 0xFFFF);
}

void Eth::DNSresolve(const char *domain){
	if(!Eth::isReady() || (domain == nullptr))
		return;

	if(m_timeoutTimer.isRunning())
		m_timeoutTimer.stopTimer();

	m_dnsInProgressFlag = true;
	m_dnsFinishedFlag = false;
	m_dnsQueryLen = 0;
	m_dnsRxLen = 0;
	m_dnsCurrentIndex = 0;

	String::strcpy(m_dnsDomain, domain);

	Eth::DNSgenerateXid();

	uint8_t aux = 0;
	for(uint8_t i = 0; i < 4; i++)	aux |= m_dhcpDNS[i];

/*	DNS provided by DHCP is not working in W5500 (idk why???)
 	But this below should be working. Meanwhile, hardcoding Google's DNS

 	if(aux == 0)	//	If DNS was NOT in DHCPOFFER, we use google's 8.8.8.8
		for(uint8_t i = 0; i < 4; i++)	m_dnsServerIP[i] = 8;
	else
		for(uint8_t i = 0; i < 4; i++)	m_dnsServerIP[i] = m_dhcpDNS[i];
*/

	//	Using Google's DNS (till I find why provided by DHCP isnt working)
	for(uint8_t i = 0; i < 4; i++)	m_dnsServerIP[i] = 8;

	for(uint16_t i = 0; i < Eth::DNS_BUFFER_LEN; i++)	m_dnsQueryBuffer[i] = 0;

	m_socketMode = socketMode_t::UDP_MODE;
	m_dnsParseState = dnsParseState_t::DNS_PARSE_NONE;
	m_ethError = ethErrorStat_t::ERROR_NONE;
	m_ethState = ethState_t::ETH_DNS_BUILD_QUERY;
}

void Eth::DNSbuildQuery(){
/*							DNS HEADER
 	+-------+-------+---------+---------+---------+---------+
	|  XID  | FLAGS | QDCOUNT | ANCOUNT | NSCOUNT | ARCOUNT |
	+-------+-------+---------+---------+---------+---------+
	 2 bytes 2 bytes  2 bytes   2 bytes   2 bytes   2 bytes

	 	 	QUESTION
	+-------+-------+--------+
	| QNAME | QTYPE | QCLASS |
	+-------+-------+--------+
	? bytes	 2 bytes  2 bytes
*/

	m_dnsCurrentIndex = 0;

	//	---------------		HEADER		---------------	//
	//	TRANSACTION ID
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = (m_dnsTransactionID >> 8);
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = (m_dnsTransactionID & 0xFF);

	//	FLAGS
	//	0x0100 = Standard query + recursion desired
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x01;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;

	//	QDCOUNT = 1 (only 1 question in this packet)
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x01;

	//	ANCOUNT = 0 (no answers yet)
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;

	//	NSCOUNT = 0	(no authority packets)
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;

	//	ARCOUNT = 0	(no additional records)
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;

	//	---------------		QUESTION		---------------	//
	//	QNAME
	uint16_t queryIndex = m_dnsCurrentIndex++;
	uint8_t labelLen = 0;
	uint16_t domainLen = String::strlen(m_dnsDomain);

	for(uint16_t i = 0; i < domainLen; i++){
		char c = m_dnsDomain[i];

		if((c == '.') || (c == '\0')){
			m_dnsQueryBuffer[queryIndex] = labelLen;

			queryIndex = m_dnsCurrentIndex++;
			labelLen = 0;
		}else{
			m_dnsQueryBuffer[m_dnsCurrentIndex++] = (uint8_t)c;
			labelLen++;
			if(labelLen > 63)	//	Max len of labels is 63 chars
				m_ethError = ethErrorStat_t::ERROR_DNS_INVALID_DOMAIN;
		}
	}
	m_dnsQueryBuffer[queryIndex] = labelLen;

	//	QNAME END
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;

	//	QTYPE:	A = IPv4
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x01;

	//	QCLASS: IN = Internet
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x00;
	m_dnsQueryBuffer[m_dnsCurrentIndex++] = 0x01;

	m_dnsQueryLen = m_dnsCurrentIndex;
}

void Eth::DNSsetRandomLocalPort(){
	static uint8_t solvingNumber = 0;

	solvingNumber++;

	uint16_t localPort;

	localPort = 0xC000 + (SysTimer::randomTick % 0xFFFF);
	if(solvingNumber < 0xF)	localPort += (solvingNumber << 2);
	else	localPort += (solvingNumber << 0);
	localPort += (m_rxBuffer[5]);	//	m_rxBuffer is never cleaned so this has random value
	localPort += (m_dnsTransactionID / (solvingNumber + 2));

	m_localPortBuffer[0] = (localPort >> 8);
	m_localPortBuffer[1] = (localPort & 0xFF);
}

void Eth::DNSsetRemoteIPandPort(){
	for(uint8_t i = 0; i < 4; i++)
		m_remoteIPBuffer[i] = m_dnsServerIP[i];

	m_remotePortBuffer[0] = (Eth::DNS_PORT >> 8);
	m_remotePortBuffer[1] = (Eth::DNS_PORT & 0xFF);
	m_destinationSetFlag = false;
}

void Eth::DNSwaitResponse(){
	m_rcvBuffer = m_dnsRxBuffer;
	m_usrAskedRcvLen = Eth::DNS_BUFFER_LEN;
	m_actualRcvLen = 0;
	m_udpPayloadLen = 0;
	m_rcvFinishedFlag = false;

	for(uint8_t i = 0; i < Eth::UDP_HEADER_LEN; i++) m_headerUDP[i] = 0;
	for(uint8_t i = 0; i < 4; i++) m_udpRcvRemoteIP[i] = 0;
	for(uint8_t i = 0; i < 2; i++) m_udpRcvRemotePort[i] = 0;
}

void Eth::DNSparseResponse(){
	switch(m_dnsParseState){
		case dnsParseState_t::DNS_PARSE_HEADER:
			if(m_dnsRxLen >= 12){	//	Min DNS header len
				//	XID
				uint16_t xID = (((uint16_t)m_dnsRxBuffer[0] << 8) |
								((uint16_t)m_dnsRxBuffer[1] << 0));

				if(xID == m_dnsTransactionID){
					uint16_t flags = (((uint16_t)m_dnsRxBuffer[2] << 8) |
									  ((uint16_t)m_dnsRxBuffer[3] << 0));

					//	FLAGS && RCODE
					if(((flags & 0x8000) != 0) && ((flags & 0x000F) == 0)){	//	QR must be 1 (this is a response)
							//	COUNTS
						m_dnsQDcount = (((uint16_t)m_dnsRxBuffer[4] << 8) |
										((uint16_t)m_dnsRxBuffer[5] << 0));

						m_dnsANcount = (((uint16_t)m_dnsRxBuffer[6] << 8) |
										((uint16_t)m_dnsRxBuffer[7] << 0));

						if((m_dnsQDcount == 1) && (m_dnsANcount > 0)){	//	Only 1 question and at least one answer
							m_dnsAnswRemaining = m_dnsANcount;
							m_dnsCurrentIndex = 12;	//	NS and AR count discarted
							m_dnsParseState = dnsParseState_t::DNS_PARSE_QUESTION_NAME;
							break;	//	Out of case => next state
						}
					}
				}
			}
			m_dnsParseState = dnsParseState_t::DNS_PARSE_ERROR;
			break;

		case dnsParseState_t::DNS_PARSE_QUESTION_NAME:
			if(m_dnsCurrentIndex < m_dnsRxLen){
				uint8_t nameByte = m_dnsRxBuffer[m_dnsCurrentIndex];

				//	NAME finished
				if(nameByte == 0x00){
					m_dnsCurrentIndex++;
					m_dnsParseState = dnsParseState_t::DNS_PARSE_QUESTION_TYPE;
					break;	//	Out of case => next state
				}else if((nameByte & 0xC0) == 0xC0){	//	Compression pointer
					if((m_dnsCurrentIndex + 1) < m_dnsRxLen){
						//	Pointer has 2 bytes len
						m_dnsCurrentIndex += 2;
						m_dnsParseState = dnsParseState_t::DNS_PARSE_QUESTION_TYPE;
						break;	//	Out of case => next state
					}
				}else if(nameByte <= 63){	//	Valid label
					if((m_dnsCurrentIndex + 1 + nameByte) <= m_dnsRxLen){
						m_dnsCurrentIndex += (1 + nameByte);	//	Skip label
						break;	//	Out of case => next state
					}
				}
			}
			m_dnsParseState = dnsParseState_t::DNS_PARSE_ERROR;
			break;

		case dnsParseState_t::DNS_PARSE_QUESTION_TYPE:
			//	QTYPE (2 bytes) + QCLASS (2 bytes)
			if((m_dnsCurrentIndex + 4) <= m_dnsRxLen){
				uint16_t qType = (((uint16_t)m_dnsRxBuffer[m_dnsCurrentIndex] << 8) |
								  ((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 1)] << 0));

				uint16_t qClass = (((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 2)] << 8) |
								   ((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 3)] << 0));

				m_dnsCurrentIndex += 4;

				if((qType == 0x0001) && (qClass == 0x0001)){	//	Requested A / IN
					m_dnsParseState = dnsParseState_t::DNS_PARSE_ANSWER_NAME;
					break;	//	Out of case => next state
				}
			}
			m_dnsParseState = dnsParseState_t::DNS_PARSE_ERROR;
			break;

		case dnsParseState_t::DNS_PARSE_ANSWER_NAME:
			if(m_dnsCurrentIndex < m_dnsRxLen){
				uint8_t nameByte = m_dnsRxBuffer[m_dnsCurrentIndex];

				//	NAME finished
				if(nameByte == 0x00){
					m_dnsCurrentIndex++;
					m_dnsParseState = dnsParseState_t::DNS_PARSE_ANSWER_FIXED;
					break;	//	Out of case => next state
				}else if((nameByte & 0xC0) == 0xC0){	//	Compression pointer
					if((m_dnsCurrentIndex + 1) < m_dnsRxLen){
						//	Pointer has 2 bytes len
						m_dnsCurrentIndex += 2;
						m_dnsParseState = dnsParseState_t::DNS_PARSE_ANSWER_FIXED;
						break;	//	Out of case => next state
					}
				}else if(nameByte <= 63){	//	Valid label
					if((m_dnsCurrentIndex + 1 + nameByte) <= m_dnsRxLen){
						m_dnsCurrentIndex += (1 + nameByte);	//	Skip label
						break;	//	Out of case => next state
					}
				}
			}
			m_dnsParseState = dnsParseState_t::DNS_PARSE_ERROR;
			break;

		case dnsParseState_t::DNS_PARSE_ANSWER_FIXED:
			/*	Type: 		2 bytes
			  	Class:		2 bytes
			  	TTL:		4 bytes
			  	RDLENGTH:	2 bytes
			*/
			if((m_dnsCurrentIndex + 10) <= m_dnsRxLen){
				m_dnsAnswType = (((uint16_t)m_dnsRxBuffer[m_dnsCurrentIndex] << 8) |
								 ((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 1)] << 0));

				m_dnsAnswClass = (((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 2)] << 8) |
								  ((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 3)] << 0));

				//	Skip TTL (4 bytes)
				m_dnsCurrentIndex += 8;

				m_dnsAnswDataLen = (((uint16_t)m_dnsRxBuffer[m_dnsCurrentIndex] << 8) |
									((uint16_t)m_dnsRxBuffer[(m_dnsCurrentIndex + 1)] << 0));

				m_dnsCurrentIndex += 2;

				//	Check RDATA exists
				if((m_dnsCurrentIndex + m_dnsAnswDataLen) <= m_dnsRxLen){
					m_dnsParseState = dnsParseState_t::DNS_PARSE_ANSWER_DATA;
					break;	//	Out of case => next state
				}
			}
			m_dnsParseState = dnsParseState_t::DNS_PARSE_ERROR;
			break;

		case dnsParseState_t::DNS_PARSE_ANSWER_DATA:
			/*	TYPE:		A
			  	CLASS:		IN
			  	RDLENGTH:	4
			*/
			if((m_dnsAnswType == 0x0001) && (m_dnsAnswClass == 0x0001) && (m_dnsAnswDataLen == 4)){
				for(uint8_t i = 0; i < 4; i++)	m_dnsResolvedIP[i] = m_dnsRxBuffer[m_dnsCurrentIndex++];
				m_dnsParseState = dnsParseState_t::DNS_PARSE_SUCCESS;
			}else{	//	This was NOT an A record, skip RDATA and inspect next answ
				m_dnsCurrentIndex += m_dnsAnswDataLen;

				if(m_dnsAnswRemaining > 0)	m_dnsAnswRemaining--;

				if(m_dnsAnswRemaining == 0){
					m_dnsParseState = dnsParseState_t::DNS_PARSE_ERROR;
				}else{
					m_dnsParseState = dnsParseState_t::DNS_PARSE_ANSWER_NAME;
				}
			}
			break;

		case dnsParseState_t::DNS_PARSE_SUCCESS:
			m_dnsInProgressFlag = false;
			break;

		case dnsParseState_t::DNS_PARSE_ERROR:
			m_dnsInProgressFlag = false;
			break;

		default:
			//	ERROR
			break;
	}
}

bool Eth::DNSresolveFinished() const{ return (Eth::isReady() && m_dnsFinishedFlag); }

uint8_t Eth::HTTPbuildBody(const char *data){
	String body(m_httpBody, Eth::HTTP_MAX_BDY_LEN);

	body += "device=";
	body += m_httpUsrAgent;

	if(data != nullptr){	//	Normal request
		body += "&path=";				//	m_httpBody = "device=[usrAgent]&path="
		body += m_httpServerDataPath;	//	m_httpBody = "device=[usrAgent]&path=[serverDataPath]"
		body += "&data=";				//	m_httpBody = "device=[usrAgent]&path=[serverDataPath]data="
		body += data;					//	m_httpBody = "device=[usrAgent]&path=[serverDataPath]data=[data]"
	}

	//	Else (data == nullptr) => Heartbeat

	if(body.getError() == String::OK){
		m_httpBodyLen = body.getLen();
	}else{
		m_httpBody[0] = '\0';
		m_httpBodyLen = 0;
	}

	return m_httpBodyLen;
}

uint16_t Eth::HTTPbuildRequest(){
	String request(m_httpRequest, Eth::HTTP_MAX_RQST_LEN);

	request += "POST ";
	request += m_httpServerPath;
	request += " HTTP/1.1\r\n";
	request += "Host: ";
	request += m_httpServerHost;
	request += "\r\n";
	request += "Content-Type: application/x-www-form-urlencoded\r\n";
	request += "Content-Length: ";
	request += m_httpBodyLen;
	request += "\r\n";

	request += "Connection: close\r\n";
	request += "User-Agent: ";
	request += m_httpUsrAgent;
	request += "\r\n";
	request += "\r\n";
	request += m_httpBody;

	if(request.getError() == String::OK){
			m_httpRequestLen = request.getLen();
		}else{
			m_httpRequest[0] = '\0';
			m_httpRequestLen = 0;
		}

	return m_httpRequestLen;
/*	REQUEST:
 *  "POST [serverPath] HTTP/1.1\r\n"
 *  "Host: [serverHost]\r\n"
 *  "Content-Type: application/x-www-form-urlencoded\r\n"
 *  "Content-Length: [m_httpBodyLen]\r\n"
 *  "Connection: close\r\n"
 *  "User-Agent: [usrAgent]\r\n"
 *  "\r\n"
 *  "device=[usrAgent]&path=[serverDataPath]&data=[data]"
 */
}

void Eth::HTTPcheckResponse(){
	if(m_httpState != httpState_t::HTTP_CHECK)
		return;

	const char *ok_10 = String::strstr(m_httpServerResponse, "HTTP/1.0 200 OK");
	const char *ok_11 = String::strstr(m_httpServerResponse, "HTTP/1.1 200 OK");

	if(ok_10 || ok_11){
		m_httpState = httpState_t::HTTP_SUCCESS;
	}else{
		m_httpError = httpError_t::HTTP_ERROR_CHECK_RESPONSE;
		m_httpState = httpState_t::HTTP_ERROR;
	}
}

void Eth::HTTPuploadData(uint16_t localPort, uint16_t serverPort, const char *serverPath, const char *serverDataPath, const char *device, const char *data){
	if(Eth::HTTPerrorOccurred() || 	m_httpHeartbeatInProgressFlag)
		return;

	m_httpInProgressFlag = true;
	m_httpFinishedFlag = false;
	m_httpErrorOccurred = false;

	for(uint8_t i = 0; i < (Eth::DNS_MAX_DOMAIN_LEN + 1); i++)	m_httpServerHost[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_SERVER_PATH_LEN; i++)	m_httpServerPath[i] = 0;
	for(uint16_t i = 0; i < Eth::HTTP_MAX_RQST_LEN; i++)	m_httpRequest[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_BDY_LEN; i++)	m_httpBody[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_USR_AGENT_LEN; i++)	m_httpUsrAgent[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_SERVER_PATH_LEN; i++)	m_httpServerDataPath[i] = 0;
	for(uint16_t i = 0; i < Eth::HTTP_MAX_RESPONSE_LEN; i++)	m_httpServerResponse[i] = 0;

	String::strcpy(m_httpServerHost, m_dnsDomain);
	String::strcpy(m_httpServerPath, serverPath);
	String::strcpy(m_httpServerDataPath, serverDataPath);
	String::strcpy(m_httpUsrAgent, device);

	m_httpServerPort = serverPort;
	m_httpServerResponseLen = 0;
	m_httpBodyLen = 0;
	m_httpRequestLen = 0;

	m_httpError = httpError_t::HTTP_ERROR_NONE;

	Eth::HTTPbuildBody(data);
	Eth::HTTPbuildRequest();

	if((m_httpBodyLen != 0) && (m_httpRequestLen != 0)){
		Eth::socketOpen(Eth::TCP_MODE, localPort);
		m_httpState = httpState_t::HTTP_CONNECT;
	}else{
		m_httpState = httpState_t::HTTP_ERROR;
		m_httpError = httpError_t::HTTP_ERROR_BUILDING;
	}
}

bool Eth::HTTPdataUploaded() const{ return m_httpFinishedFlag; }

void Eth::HTTPheartbeat(uint16_t localPort, uint16_t serverPort, const char *serverPath, const char *device){
	if(Eth::HTTPerrorOccurred() || m_httpInProgressFlag)
		return;

	m_httpInProgressFlag = true;
	m_httpFinishedFlag = false;
	m_httpHeartbeatInProgressFlag = true;
	m_httpHeartbeatFinishedFlag = false;
	m_httpErrorOccurred = false;

	for(uint8_t i = 0; i < (Eth::DNS_MAX_DOMAIN_LEN + 1); i++)	m_httpServerHost[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_SERVER_PATH_LEN; i++)	m_httpServerPath[i] = 0;
	for(uint16_t i = 0; i < Eth::HTTP_MAX_RQST_LEN; i++)	m_httpRequest[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_BDY_LEN; i++)	m_httpBody[i] = 0;
	for(uint8_t i = 0; i < Eth::HTTP_MAX_USR_AGENT_LEN; i++)	m_httpUsrAgent[i] = 0;

	String::strcpy(m_httpServerHost, m_dnsDomain);
	String::strcpy(m_httpServerPath, serverPath);
	String::strcpy(m_httpUsrAgent, device);

	m_httpServerPort = serverPort;
	m_httpBodyLen = 0;
	m_httpRequestLen = 0;

	m_httpError = httpError_t::HTTP_ERROR_NONE;

	Eth::HTTPbuildBody(nullptr);	//	Builds body for heartbeat
	Eth::HTTPbuildRequest();

	if((m_httpBodyLen != 0) && (m_httpRequestLen != 0)){
		Eth::socketOpen(Eth::TCP_MODE, localPort);
		m_httpState = httpState_t::HTTP_CONNECT;
	}else{
		m_httpState = httpState_t::HTTP_ERROR;
		m_httpError = httpError_t::HTTP_ERROR_BUILDING;
	}
}

bool Eth::HTTPerrorOccurred() const{ return m_httpErrorOccurred; }

void Eth::HTTPuploadData(){
	if(!Eth::isReady() || !m_httpInProgressFlag || 	m_httpHeartbeatInProgressFlag)
		return;

	switch(m_httpState){
		case httpState_t::HTTP_IDLE:
			//	Nothing to do
			break;

		case httpState_t::HTTP_CONNECT:
			if(Eth::socketOpened()){
				Eth::socketTCPconnect(m_httpServerPort);
				m_httpState = httpState_t::HTTP_SEND;
			}
			break;

		case httpState_t::HTTP_SEND:
			if(Eth::socketTCPconnected()){
				Eth::socketTCPsend(m_httpRequest, m_httpRequestLen);
				m_httpState = httpState_t::HTTP_RCV;
			}
			break;

		case httpState_t::HTTP_RCV:
			if(Eth::socketSendFinished()){
				Eth::socketTCPreceive(m_httpServerResponse, Eth::HTTP_MAX_RESPONSE_LEN);
				m_httpState = httpState_t::HTTP_CHECK;
			}
			break;

		case httpState_t::HTTP_CHECK:
			if(Eth::socketReceiveFinished()){
				m_httpServerResponseLen = Eth::socketReceivedLen();
				Eth::HTTPcheckResponse();
			}
			break;

		case httpState_t::HTTP_SUCCESS:
			Eth::socketTCPdisconnect();
			m_httpState = httpState_t::HTTP_FINISHED;
			break;

		case httpState_t::HTTP_FINISHED:
			m_httpInProgressFlag = false;
			m_httpFinishedFlag = true;
			m_httpState = httpState_t::HTTP_IDLE;
			break;

		case httpState_t::HTTP_ERROR:
			m_httpErrorOccurred = true;
			m_httpInProgressFlag = false;
			m_httpFinishedFlag = false;
			m_httpState = httpState_t::HTTP_IDLE;
			break;

		default:
			//	ERROR
			break;
	}
}

bool Eth::HTTPheartbeatFinished() const{ return m_httpHeartbeatFinishedFlag; }

void Eth::HTTPheartbeat(){
	if(!Eth::isReady() || !m_httpInProgressFlag || 	!m_httpHeartbeatInProgressFlag)
		return;

	switch(m_httpState){
		case httpState_t::HTTP_IDLE:
			//	Nothing to do
			break;

		case httpState_t::HTTP_CONNECT:
			if(Eth::socketOpened()){
				Eth::socketTCPconnect(m_httpServerPort);
				m_httpState = httpState_t::HTTP_SEND;
			}
			break;

		case httpState_t::HTTP_SEND:
			if(Eth::socketTCPconnected()){
				Eth::socketTCPsend(m_httpRequest, m_httpRequestLen);
				m_httpState = httpState_t::HTTP_RCV;
			}
			break;

		case httpState_t::HTTP_RCV:
			if(Eth::socketSendFinished()){
				Eth::socketTCPreceive(m_httpServerResponse, Eth::HTTP_MAX_RESPONSE_LEN);
				m_httpState = httpState_t::HTTP_CHECK;
			}
			break;

		case httpState_t::HTTP_CHECK:
			if(Eth::socketReceiveFinished()){
				m_httpServerResponseLen = Eth::socketReceivedLen();
				Eth::HTTPcheckResponse();
			}
			break;

		case httpState_t::HTTP_SUCCESS:
			Eth::socketTCPdisconnect();
			m_httpState = httpState_t::HTTP_FINISHED;
			break;

		case httpState_t::HTTP_FINISHED:
			m_httpInProgressFlag = false;
			m_httpHeartbeatInProgressFlag = false;
			m_httpHeartbeatFinishedFlag = true;
			m_httpState = httpState_t::HTTP_IDLE;
			break;

		case httpState_t::HTTP_ERROR:
			m_httpErrorOccurred = true;
			m_httpInProgressFlag = false;
			m_httpHeartbeatInProgressFlag = false;
			m_httpHeartbeatFinishedFlag = false;
			m_httpFinishedFlag = false;
			m_httpState = httpState_t::HTTP_IDLE;
			break;

		default:
			//	ERROR
			break;
	}
}

void Eth::HTTPtimeoutError(ethState_t currentEthState){
	switch(currentEthState){
		case ethState_t::ETH_SOCKET_OPEN_CHECK_STATUS:
			m_httpError = httpError_t::HTTP_ERROR_OPEN_TIMEOUT;
			m_httpState = httpState_t::HTTP_ERROR;
			break;

		case ethState_t::ETH_SOCKET_CONNECT_CHECK_STATUS:
			m_httpError = httpError_t::HTTP_ERROR_CONNECT_TIMEOUT;
			m_httpState = httpState_t::HTTP_ERROR;
			break;

		case ethState_t::ETH_SOCKET_SEND_TCP_WAIT_TX_FSR:
			m_httpError = httpError_t::HTTP_ERROR_SEND_TIMEOUT;
			m_httpState = httpState_t::HTTP_ERROR;
			Eth::socketTCPdisconnect();
			break;

		case ethState_t::ETH_SOCKET_SEND_WAIT_READ_INTERRUPT_STAT:
			m_httpError = httpError_t::HTTP_ERROR_SEND_TIMEOUT;
			m_httpState = httpState_t::HTTP_ERROR;
			Eth::socketTCPdisconnect();
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RSR:
			m_httpError = httpError_t::HTTP_ERROR_RCV_TIMEOUT;
			m_httpState = httpState_t::HTTP_ERROR;
			Eth::socketTCPdisconnect();
			break;

		case ethState_t::ETH_SOCKET_DISCONNECT_CHECK_STATUS:
			m_httpError = httpError_t::HTTP_ERROR_NONE;
			m_httpState = httpState_t::HTTP_FINISHED;
			break;

		default:
			//	ERROR
			break;
	}
}

void Eth::HTTPuploading(bool flag){ m_httpUploading_usrFlag = flag; }

bool Eth::HTTPuploading() const{ return m_httpUploading_usrFlag; }

void Eth::HTTPheartBeating(bool flag){ m_httpHeartBeating_usrFlag = flag; }

bool Eth::HTTPheartBeating() const{ return m_httpHeartBeating_usrFlag; }

bool Eth::HTTPisBusy() const{
	return (!Eth::DNSresolveFinished() || Eth::HTTPuploading() || Eth::HTTPheartBeating());
}

void Eth::HTTPrestartAfterError(){
	m_httpInProgressFlag = false;
	m_httpHeartbeatInProgressFlag = false;
	m_httpFinishedFlag = false;
	m_httpErrorOccurred = false;
	Eth::HTTPuploading(false);
	Eth::HTTPheartBeating(false);

	m_httpError = httpError_t::HTTP_ERROR_NONE;
	m_httpState = httpState_t::HTTP_IDLE;

	m_ethState = ethState_t::ETH_IDLE;
}

void Eth::SPItransferHandler(){
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
}

void Eth::W5500configStateMachine(){
	switch(m_currentConfigStat){
		case configState_t::CONFIG_MAC:
			Eth::writeBuffer(registerAddr_t::SHAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_mac, 6, &m_socketTransferDone);
			break;

		case configState_t::CONFIG_RX_BUFFER_SIZE:
			Eth::writeByte(registerAddr_t::Sn_RXBUF_SIZE_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, (uint8_t)(m_rxBufferSize / 1024), &m_socketTransferDone);
			break;

		case configState_t::CONFIG_TX_BUFFER_SIZE:
			Eth::writeByte(registerAddr_t::Sn_TXBUF_SIZE_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, (uint8_t)(m_txBufferSize / 1024), &m_socketTransferDone);
			break;

		case configState_t::CONFIG_IP:
			Eth::writeBuffer(registerAddr_t::SIPR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_ip, 4, &m_socketTransferDone);
			break;

		case configState_t::CONFIG_GATEWAY:
			Eth::writeBuffer(registerAddr_t::GAR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_gateway, 4, &m_socketTransferDone);
			break;

		case configState_t::CONFIG_SUBNET:
			Eth::writeBuffer(registerAddr_t::SUBR0_REGISTER, block_t::COMMON_REGISTER_BLOCK, m_subnet, 4, &m_socketTransferDone);
			break;

		default:
			//	ERROR
			break;
	}
}

void Eth::W5500selectNextConfigState(){
	switch(m_currentConfigStat){
		case configState_t::CONFIG_MAC:
			m_currentConfigStat = configState_t::CONFIG_RX_BUFFER_SIZE;
			m_ethState = ethState_t::ETH_CONFIG_WRITE;
			break;

		case configState_t::CONFIG_RX_BUFFER_SIZE:
			m_currentConfigStat = configState_t::CONFIG_TX_BUFFER_SIZE;
			m_ethState = ethState_t::ETH_CONFIG_WRITE;
			break;

		case configState_t::CONFIG_TX_BUFFER_SIZE:
			m_currentConfigStat = configState_t::CONFIG_IP;
			if(m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP)
				m_ethState = ethState_t::ETH_DHCP_START;
			else if(m_initConfigMode == initConfigMode_t::INIT_WITH_STATIC_IP)
				m_ethState = ethState_t::ETH_CONFIG_WRITE;
			break;

		case configState_t::CONFIG_IP:
			m_currentConfigStat = configState_t::CONFIG_GATEWAY;
			m_ethState = ethState_t::ETH_CONFIG_WRITE;
			break;

		case configState_t::CONFIG_GATEWAY:
			m_currentConfigStat = configState_t::CONFIG_SUBNET;
			m_ethState = ethState_t::ETH_CONFIG_WRITE;
			break;

		case configState_t::CONFIG_SUBNET:
			m_currentConfigStat = configState_t::CONFIG_NONE;
			m_ethState = ethState_t::ETH_CONFIG_FINISHED;
			break;

		default:
			//	ERROR
			break;
	}
}

void Eth::stateMachine(){

	//	SPI transaction handler
	Eth::SPItransferHandler();

	//	HTTP heartbeat handler
	Eth::HTTPheartbeat();

	//	HTTP upload data handler
	Eth::HTTPuploadData();

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

			Eth::W5500configStateMachine();

			m_ethState = ethState_t::ETH_CONFIG_WAIT_WRITE;

			break;

		case ethState_t::ETH_CONFIG_WAIT_WRITE:
			if(m_socketTransferDone){
				m_ethState = ethState_t::ETH_CONFIG_NEXT;
			}
			break;

		case ethState_t::ETH_CONFIG_NEXT:
			Eth::W5500selectNextConfigState();
			break;


		case ethState_t::ETH_CONFIG_FINISHED:
			m_initFinishedFlag = true;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	INIT DHCP CONFIG	-------------------------	//


		case ethState_t::ETH_DHCP_START:
			Eth::DHCPstart();
			m_ethState = ethState_t::ETH_SOCKET_OPEN_WRITE_MODE;
			break;

		case ethState_t::ETH_DHCP_BUILD_DISCOVER:
			Eth::DHCPbuildDiscover();
			m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
			break;

		case ethState_t::ETH_DHCP_WAIT_OFFER:
			Eth::DHCPwaitOffer();
			m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;
			break;

		case ethState_t::ETH_DHCP_PARSE_OFFER:
			if(Eth::DHCPparseOffer()){
				m_ethState = ethState_t::ETH_DHCP_VALIDATE_OPTIONS;
			}else{
				m_ethState = ethState_t::ETH_DHCP_WAIT_OFFER;
			}
			break;

		case ethState_t::ETH_DHCP_VALIDATE_OPTIONS:
			if(m_dhcpState == dhcpState_t::DHCP_PARSE_OFFER){
				Eth::DHCPvalidateOption(Eth::DHCP_OFFER, ethState_t::ETH_DHCP_VALIDATE_OFFER);
				if(m_dhcpOptionErrorFlag)	m_ethState = ethState_t::ETH_DHCP_WAIT_OFFER;

			}else if(m_dhcpState == dhcpState_t::DHCP_PARSE_ACK){
				Eth::DHCPvalidateOption(Eth::DHCP_ACK, ethState_t::ETH_DHCP_VALIDATE_ACK);
				if(m_dhcpOptionErrorFlag)	m_ethState = ethState_t::ETH_DHCP_WAIT_ACK;
			}
			break;

		case ethState_t::ETH_DHCP_VALIDATE_OFFER:
			if(!m_dhcpMsgTypeFound_flag || !m_dhcpServerIpFound_flag){
				m_ethState = ethState_t::ETH_DHCP_WAIT_OFFER;	//	Subnet, gateway & DNS can be (or not) inside DHCPOFFER
			}else{
				m_ethState = ethState_t::ETH_DHCP_BUILD_REQUEST;
			}
			break;

		case ethState_t::ETH_DHCP_BUILD_REQUEST:
			Eth::DHCPbuildRequest();
			m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
			break;

		case ethState_t::ETH_DHCP_WAIT_ACK:
			Eth::DHCPwaitACK();
			m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;
			break;

		case ethState_t::ETH_DHCP_PARSE_ACK:
			if(Eth::DHCPparseACK()){
				m_ethState = ethState_t::ETH_DHCP_VALIDATE_OPTIONS;
			}else{
				m_ethState = ethState_t::ETH_DHCP_WAIT_ACK;
			}
			break;

		case ethState_t::ETH_DHCP_VALIDATE_ACK:
			if(m_dhcpOptionNACKfound){	//	Server NACKed DHCPREQUEST
				m_ethState = ethState_t::ETH_DHCP_START;	//	Re-start DHCP
			}else if(!m_dhcpMsgTypeFound_flag   ||
					 !m_dhcpLeaseTimeFound_flag ||
					 !m_dhcpServerIpFound_flag	||
					 !m_dhcpSubnetFound_flag){
				m_ethState = ethState_t::ETH_DHCP_WAIT_ACK;	//	Gateway & DNS can be (or not) inside DHCPACK
			}else{
				m_ethState = ethState_t::ETH_DHCP_FINISHED;
			}
			break;

		case ethState_t::ETH_DHCP_FINISHED:
			for(uint8_t i = 0; i < 4; i++) m_ip[i] = m_dhcpOfferedIP[i];
			m_dhcpOptionErrorFlag = false;
			m_dhcpInProgressFlag = false;
			m_dhcpFinishedFlag = true;
			m_dhcpState = dhcpState_t::DHCP_FINISHED;
			m_ethState = ethState_t::ETH_CONFIG_WRITE;
			break;


		//	-------------------------	DNS	-------------------------	//

		case ethState_t::ETH_DNS_BUILD_QUERY:
			Eth::DNSbuildQuery();
			if(Eth::currentError() != Eth::ERROR_DNS_INVALID_DOMAIN){
				Eth::DNSsetRandomLocalPort();

				m_ethState = ethState_t::ETH_SOCKET_OPEN_WRITE_MODE;
			}else{
				m_dnsInProgressFlag = false;
				m_ethState = ethState_t::ETH_IDLE;
			}
			break;

		case ethState_t::ETH_DNS_SEND_QUERY:
			m_sendBuffer = m_dnsQueryBuffer;
			m_sendLen = m_dnsQueryLen;
			m_sendProcessedLen = 0;
			m_sendCurrentLen = 0;
			m_sendRemainingLen = 0;
			m_sendFinishedFlag = false;

			m_ethState = ethState_t::ETH_SOCKET_SEND_READ_TX_FSR;
			break;

		case ethState_t::ETH_DNS_WAIT_RESPONSE:
			Eth::DNSwaitResponse();
			m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;
			break;

		case ethState_t::ETH_DNS_PARSE_RESPONSE:
			Eth::DNSparseResponse();
			if(m_dnsParseState == dnsParseState_t::DNS_PARSE_ERROR){
				m_dnsInProgressFlag = false;
				m_ethState = ethState_t::ETH_IDLE;
			}else if(m_dnsParseState == dnsParseState_t::DNS_PARSE_SUCCESS){
				m_ethState = ethState_t::ETH_DNS_FINISHED;
			}
			break;

		case ethState_t::ETH_DNS_FINISHED:
			m_dnsInProgressFlag = false;
			m_dnsFinishedFlag = true;
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
				if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
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
			if(m_timeoutTimer.isRunning())
				m_timeoutTimer.stopTimer();
			m_socketTransferDone = false;

			if(m_dhcpInProgressFlag && (m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP)){
				m_destinationSetFlag = false;
				m_ethState = ethState_t::ETH_SOCKET_WRITE_IP;
			}else if(m_dnsInProgressFlag){
				Eth::DNSsetRemoteIPandPort();
				m_ethState = ethState_t::ETH_SOCKET_WRITE_IP;
			}else{
				m_ethState = ethState_t::ETH_IDLE;
			}
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
					if(m_dhcpInProgressFlag && (m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP)){
						m_ethState = ethState_t::ETH_SOCKET_CONNECT_FINISHED;
					}else{
						m_destinationSetFlag = true;
						m_ethState = ethState_t::ETH_SOCKET_CONNECT_FINISHED;
					}
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
				if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
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
				if(m_httpInProgressFlag){
					m_httpError = httpError_t::HTTP_ERROR_CONN_SOCK_CLOSED;
					m_httpState = httpState_t::HTTP_ERROR;
				}
				m_ethState = ethState_t::ETH_IDLE;
			}else if(m_socketStat == socketStat_t::SOCK_EXCEPTION_TRANSIENT_STAT){
				Eth::socketRequestStatus(ethState_t::ETH_SOCKET_CONNECT_CHECK_STATUS);
			}
			break;

		case ethState_t::ETH_SOCKET_CONNECT_FINISHED:
			if(m_timeoutTimer.isRunning())
				m_timeoutTimer.stopTimer();
			m_socketTransferDone = false;

			if(m_dhcpInProgressFlag && (m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP))
				m_ethState = ethState_t::ETH_DHCP_BUILD_DISCOVER;
			else if(m_dnsInProgressFlag)
				m_ethState = ethState_t::ETH_DNS_SEND_QUERY;
			else
				m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	SEND SOCKET (TCP & UDP)	 -------------------------	//


		case ethState_t::ETH_SOCKET_SEND_READ_TX_FSR:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_TX_FSR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_txFreeSize, 2, &m_socketTransferDone);
			if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
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
			if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
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
			if(m_timeoutTimer.isRunning())
				m_timeoutTimer.stopTimer();
			m_socketTransferDone = false;
			m_sendFinishedFlag = true;

			if(m_dhcpInProgressFlag && (m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP)){
				if(m_dhcpState == dhcpState_t::DHCP_BUILD_DISCOVER)
					m_ethState = ethState_t::ETH_DHCP_WAIT_OFFER;
				else if(m_dhcpState == dhcpState_t::DHCP_BUILD_REQUEST)
					m_ethState = ethState_t::ETH_DHCP_WAIT_ACK;

			}else if(m_dnsInProgressFlag){
				m_ethState = ethState_t::ETH_DNS_WAIT_RESPONSE;

			}else{
				m_ethState = ethState_t::ETH_IDLE;
			}
			break;


		//	-------------------------	RECEIVE SOCKET (TCP & UDP)	 -------------------------	//


		case ethState_t::ETH_SOCKET_RCV_READ_RX_RSR:
			m_socketTransferDone = false;
			Eth::readBuffer(registerAddr_t::Sn_RX_RSR0_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, m_rxReceivedSize, 2, &m_socketTransferDone);
			if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
				m_timeoutTimer.startTimer();	//	5s TIMEOUT
			m_ethState = ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RSR;
			break;

		case ethState_t::ETH_SOCKET_RCV_WAIT_READ_RX_RSR:
			if(m_timeoutTimer.singleTimerExpired()){
				if(m_dhcpInProgressFlag && (m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP)){
					m_dhcpRetryCount++;
					if(m_dhcpRetryCount >= 5){
						Eth::timeoutError();
					}else{
						m_timeoutTimer.stopTimer();
						m_ethState = ethState_t::ETH_DHCP_BUILD_DISCOVER;
					}
				}else{
					Eth::timeoutError();
				}
				break;
			}
			if(m_socketTransferDone){
				m_actualRcvLen = ((m_rxReceivedSize[0] << 8) | m_rxReceivedSize[1]);
				if(m_actualRcvLen == 0 || ((m_socketMode == socketMode_t::UDP_MODE) && (m_actualRcvLen < Eth::UDP_HEADER_LEN))){
					m_ethState = ethState_t::ETH_SOCKET_RCV_READ_RX_RSR;	//	No Data was Received Yet
				}else{
					m_timeoutTimer.stopTimer();
					if((m_actualRcvLen > m_usrAskedRcvLen) && (m_socketMode == socketMode_t::TCP_MODE)){
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
			if(m_socketTransferDone){
				if(m_socketMode == socketMode_t::TCP_MODE)
					m_ethState = ethState_t::ETH_SOCKET_RCV_TCP_READ_BUFFER;
				else if(m_socketMode == socketMode_t::UDP_MODE)
					m_ethState = ethState_t::ETH_SOCKET_RCV_UDP_READ_HEADER;
			}
			break;


		//	-------------------------	RECEIVE SOCKET (TCP)	-------------------------	//


		case ethState_t::ETH_SOCKET_RCV_TCP_READ_BUFFER:
			m_socketTransferDone = false;
			Eth::readBuffer(((m_rxReadPointer[0] << 8) | m_rxReadPointer[1]), block_t::SOCKET0_RX_BUFFER_BLOCK, m_rcvBuffer, ((m_rxReceivedSize[0] << 8) | m_rxReceivedSize[1]), &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_TCP_WAIT_READ_BUFFER;
			break;

		case ethState_t::ETH_SOCKET_RCV_TCP_WAIT_READ_BUFFER:
			if(m_socketTransferDone){
				m_nextRxReadPointer = (((m_rxReadPointer[0] << 8) | m_rxReadPointer[1]) + ((m_rxReceivedSize[0] << 8) | m_rxReceivedSize[1]));
				m_ethState = ethState_t::ETH_SOCKET_RCV_WRITE_RX_RD;
			}
			break;


		//	-------------------------	RECEIVE SOCKET (UDP)	-------------------------	//


		case ethState_t::ETH_SOCKET_RCV_UDP_READ_HEADER:
			m_socketTransferDone = false;
			Eth::readBuffer(((m_rxReadPointer[0] << 8) | m_rxReadPointer[1]), block_t::SOCKET0_RX_BUFFER_BLOCK, m_headerUDP, Eth::UDP_HEADER_LEN, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_RCV_UDP_WAIT_READ_HEADER;
			break;

		case ethState_t::ETH_SOCKET_RCV_UDP_WAIT_READ_HEADER:
			if(m_socketTransferDone){
				for(uint8_t i = 0; i < 4; i++) m_udpRcvRemoteIP[i] = m_headerUDP[i];
				for(uint8_t i = 0; i < 2; i++) m_udpRcvRemotePort[i] = m_headerUDP[(i+4)];

				m_udpPayloadLen = ((m_headerUDP[6] << 8) | m_headerUDP[7]);

				m_ethState = ethState_t::ETH_SOCKET_RCV_UDP_READ_PAYLOAD;
			}
			break;

		case ethState_t::ETH_SOCKET_RCV_UDP_READ_PAYLOAD:{
			uint16_t currentRxReadPointer = ((m_rxReadPointer[0] << 8) | m_rxReadPointer[1]);
			if(m_udpPayloadLen == 0){
				m_actualRcvLen = 0;
				m_nextRxReadPointer = (currentRxReadPointer + Eth::UDP_HEADER_LEN);
				m_ethState = ethState_t::ETH_SOCKET_RCV_WRITE_RX_RD;
			}else{
				uint16_t readBufferLen = ((m_udpPayloadLen > m_usrAskedRcvLen) ? m_usrAskedRcvLen : m_udpPayloadLen); 				//	True if user asked less data than received from server
				m_actualRcvLen = m_udpPayloadLen;
				m_nextRxReadPointer = (currentRxReadPointer + m_udpPayloadLen + Eth::UDP_HEADER_LEN);	//	All data has to be consumed in UDP

				m_socketTransferDone = false;
				Eth::readBuffer((currentRxReadPointer + Eth::UDP_HEADER_LEN), block_t::SOCKET0_RX_BUFFER_BLOCK, m_rcvBuffer, readBufferLen, &m_socketTransferDone);
				m_ethState = ethState_t::ETH_SOCKET_RCV_UDP_WAIT_READ_PAYLOAD;
			}
			break;
		}

		case ethState_t::ETH_SOCKET_RCV_UDP_WAIT_READ_PAYLOAD:
			if(m_socketTransferDone)	m_ethState = ethState_t::ETH_SOCKET_RCV_WRITE_RX_RD;
			break;


		//	-------------------------	RECEIVE SOCKET (TCP & UDP)	 -------------------------	//


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
			if(m_socketTransferDone){
				if(m_socketMode == socketMode_t::TCP_MODE)
					m_ethState = ethState_t::ETH_SOCKET_RCV_CLEAR_INTERRUPT;
				else if(m_socketMode == socketMode_t::UDP_MODE)
					m_ethState = ethState_t::ETH_SOCKET_RCV_FINISHED;
			}
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
			if(m_timeoutTimer.isRunning())
				m_timeoutTimer.stopTimer();
			m_socketTransferDone = false;
			m_rcvFinishedFlag = true;
			if(m_dhcpInProgressFlag && (m_initConfigMode == initConfigMode_t::INIT_WITH_DHCP)){
				m_dhcpRxLen = m_actualRcvLen;
				m_dhcpCurrentRxIndex = 0;
				if(m_dhcpState == dhcpState_t::DHCP_WAIT_OFFER)
					m_ethState = ethState_t::ETH_DHCP_PARSE_OFFER;
				else if(m_dhcpState == dhcpState_t::DHCP_WAIT_ACK)
					m_ethState = ethState_t::ETH_DHCP_PARSE_ACK;

			}else if(m_dnsInProgressFlag){
				m_dnsRxLen = m_actualRcvLen;
				m_dnsCurrentIndex = 0;
				m_dnsParseState = dnsParseState_t::DNS_PARSE_HEADER;
				m_ethState = ethState_t::ETH_DNS_PARSE_RESPONSE;

			}else{
				m_ethState = ethState_t::ETH_IDLE;
			}
			break;


		//	-------------------------	DISCONNECT SOCKET (TCP)	 -------------------------	//


		case ethState_t::ETH_SOCKET_DISCONNECT_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::DISCONNECT_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_DISCONNECT_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_DISCONNECT_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone){
				if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
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
			if(m_timeoutTimer.isRunning())
				m_timeoutTimer.stopTimer();
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;


		//	-------------------------	CLOSE SOCKET (TCP & UDP)	-------------------------	//


		case ethState_t::ETH_SOCKET_CLOSE_CLEAR_INTERRUPT:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_IR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, (uint8_t)(0xF | (1 << 4)), &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_CLOSE_WAIT_CLEAR_INTERRUPT;
			break;

		case ethState_t::ETH_SOCKET_CLOSE_WAIT_CLEAR_INTERRUPT:
			if(m_socketTransferDone) m_ethState = ethState_t::ETH_SOCKET_CLOSE_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_CLOSE_WRITE_COMMAND:
			m_socketTransferDone = false;
			Eth::writeByte(registerAddr_t::Sn_CR_REGISTER, block_t::SOCKET0_REGISTER_BLOCK, socketCmd_t::CLOSE_SOCKET, &m_socketTransferDone);
			m_ethState = ethState_t::ETH_SOCKET_CLOSE_WAIT_WRITE_COMMAND;
			break;

		case ethState_t::ETH_SOCKET_CLOSE_WAIT_WRITE_COMMAND:
			if(m_socketTransferDone){
				if(!m_timeoutTimer.isRunning() && !m_timeoutTimer.singleTimerExpired())
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
			if(m_timeoutTimer.isRunning())
				m_timeoutTimer.stopTimer();
			m_socketTransferDone = false;
			m_ethState = ethState_t::ETH_IDLE;
			break;

		default:
			// ERROR
			break;
	}
}

Eth::~Eth(){}
