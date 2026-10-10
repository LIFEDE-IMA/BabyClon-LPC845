/*
 * bt.cpp
 *
 *  Created on: 8 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This class was written to handle HC-05 BT Module
 */

#include "bt.h"

BT::BT(Uart &uart, uint8_t headerByte, uint8_t footerByte, bool crlf, bool cr, bool lf) : m_uart(uart){
	m_readIndex = 0;

	if(headerByte != BT::NO_HEADER)	BT::setHeaderByte(headerByte);
	if(footerByte != BT::NO_FOOTER)	BT::setFooterByte(footerByte);
	if(crlf)	BT::setCRLF();
	if(cr)	BT::setCR();
	if(lf)	BT::setLF();
}

void BT::setHeaderByte(uint8_t headerByte){ m_uart.setHeaderByte(headerByte); }

void BT::clrHeaderByte(void){ m_uart.clrHeaderByte(); }

void BT::setFooterByte(uint8_t footerByte){ m_uart.setFooterByte(footerByte); }

void BT::clrFooterByte(void){ m_uart.clrFooterByte(); }

void BT::setCRLF(void){ m_uart.setCRLF(); }

void BT::clrCRLF(void){ m_uart.clrCRLF(); }

void BT::setCR(void){ m_uart.setCR(); }

void BT::clrCR(void){ m_uart.clrCR(); }

void BT::setLF(void){ m_uart.setLF(); }

void BT::clrLF(void){ m_uart.clrLF(); }

int16_t BT::readData(char *readBuff, uint8_t maxLen){
	if((readBuff == nullptr) || (maxLen < 2))	return -1;

	int16_t data = m_uart.receiveByte();

	if(data == Uart::NO_DATA_RECEIVED)	return data;

	if((data == '\r') || (data == '\n')){
		if(m_readIndex == 0)	return 	Uart::NO_DATA_RECEIVED;	//	Empty Line
	}else{
		readBuff[m_readIndex++] = (char)data;
		if(m_readIndex < (maxLen - 1))	return Uart::NO_DATA_RECEIVED;	//	Not \r \n or maxLen
	}

	readBuff[m_readIndex] = '\0';
	int16_t len = m_readIndex;
	m_readIndex = 0;
	return len;
}

bool BT::sendData(const char *sendBuff){
	return m_uart.sendStr(sendBuff);
}

BT::~BT(){}
