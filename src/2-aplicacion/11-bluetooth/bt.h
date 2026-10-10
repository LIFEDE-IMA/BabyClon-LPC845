/*
 * bt.h
 *
 *  Created on: 8 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This class was written to handle HC-05 BT Module
 */

#ifndef BT_H_
#define BT_H_

#include "uart.h"
#include "my_string.h"

class BT{
	public:
		static const uint8_t NO_HEADER = 255;
		static const uint8_t NO_FOOTER = 255;

	private:
		Uart &m_uart;

		uint8_t m_readIndex;

	public:
		BT(Uart &uart, uint8_t headerByte = BT::NO_HEADER, uint8_t footerByte = BT::NO_FOOTER, bool crlf = false, bool cr = false, bool lf = false);	//	Constructor

		void setHeaderByte(uint8_t headerByte);
		void clrHeaderByte(void);
		void setFooterByte(uint8_t footerByte);
		void clrFooterByte(void);

		void setCRLF(void);
		void clrCRLF(void);
		void setCR(void);
		void clrCR(void);
		void setLF(void);
		void clrLF(void);

		int16_t readData(char *readBuff, uint8_t maxLen);	//	Reads Received Data
		bool sendData(const char *sendBuff);			//	Sends Data

		~BT();	//	Destructor
};

#endif /* BT_H_ */
