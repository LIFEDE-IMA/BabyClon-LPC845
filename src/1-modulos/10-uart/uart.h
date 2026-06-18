/*
 * uart.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#ifndef UART_H_
#define UART_H_

#include "LPC845.h"

#if defined (__cplusplus)
extern "C" {

void UART0_IRQHandler(void);
void UART1_IRQHandler(void);
void UART2_IRQHandler(void);

}
#endif

#define TOTAL_UART_SOURCES 5

#define MAX_RX_LEN 128
#define MAX_TX_LEN 128

class Uart{
	private:
		UART_Type* m_uart;

		uint8_t m_bufferRx[MAX_RX_LEN];
		uint32_t m_maxRxLen;
		volatile uint32_t m_headRxIndex;	//	Circular buffer
		volatile uint32_t m_tailRxIndex;	//	Circular buffer
		uint8_t m_bufferTx[MAX_TX_LEN];
		uint32_t m_maxTxLen;
		volatile uint32_t m_headTxIndex;	//	Circular buffer
		volatile uint32_t m_tailTxIndex;	//	Circular buffer
		volatile bool m_flagTx;				//	Filling / Not Filling Tx Buffer
		uint8_t m_matchIndex;

		void init(uint8_t uart);			//	Initializes UARTx
		void isrHandler();					//	UARTx ISR handler

	public:
		Uart(uint8_t uart);		//	Constructor

		void pushRx(uint8_t data);						//	ISR pushes data into rx buffer
		bool popRx(uint8_t *data);						//	App pops data from rx buffer
		bool pushTx(uint8_t data);						//	App pushes data into tx buffer
		bool popTx(uint8_t *data);						//	ISR pops data from tx buffer
		bool sendStr(const char *msg);					//	Transmit string
		uint8_t readStr(char *msg, uint32_t maxLen);	//	Read received string

		friend void UART0_IRQHandler(void);	//	ISR Handler for	UART0
		friend void UART1_IRQHandler(void);	//	ISR Handler for	UART1
		friend void UART2_IRQHandler(void);	//	ISR Handler for	UART2

		~Uart();						//	Destructor
};


#endif /* UART_H_ */
