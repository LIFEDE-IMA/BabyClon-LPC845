/*
 * uart.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "uart.h"

UART_Type* UARTS[] = {UART0, UART1, UART2, UART3, UART4};

Uart *uartInstance[TOTAL_UART_SOURCES] = {nullptr, nullptr, nullptr, nullptr, nullptr};

Uart::Uart(uint8_t uart) : m_uart(UARTS[uart]){
	m_maxRxLen = MAX_RX_LEN;
	for(uint8_t i = 0; i < m_maxRxLen; i++) m_bufferRx[i] = 0;
	m_headRxIndex = 0;
	m_tailRxIndex = 0;
	m_maxTxLen = MAX_TX_LEN;
	for(uint8_t i = 0; i < m_maxTxLen; i++) m_bufferTx[i] = 0;
	m_headTxIndex = 0;
	m_tailTxIndex = 0;
	m_flagTx = false;
	m_sendPtr = nullptr;
	m_readIndex = 0;

	uartInstance[uart] = this;

	Uart::init(uart);
}

void Uart::init(uint8_t uart){	//	NVIC (Cap. 7), SYSCON (Cap. 8), SWM (Cap. 10), UART (Cap. 17)
	SYSCON->FCLKSEL[uart] = 0x1;	//	Clk source for UARTx = main (30MHz)

	switch(uart){
		case 0:
			SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 14));	//	Enables clk for SWM, UART0
			//	Reset UART0
			SYSCON->PRESETCTRL0 &= ~(1 << 14);	//	RESET UART0
			SYSCON->PRESETCTRL0 |= (1 << 14);	//	RELEASE RESET UART0
			//	Assign UART pins (PIO0_26 RXD, PIO0_27 TXD)
			SWM->PINASSIGN[0] &= ~((0xFF << 0) | (0xFF << 8));
			SWM->PINASSIGN[0] |= ((27 << 0) | (26 << 8));	//	Assigns RXD and TXD
			//	Config UART0
			UART0->CFG = (0 << 0);	//	Disables UART0
			UART0->CFG |= ((1 << 2) | (0 << 4) | (0 << 6) | (0 << 9) | (0 << 11) | (0 << 15));	//	8-bit, No-parity, 1-stopBit, No-flowControl, Asincronic, No-loop
			UART0->OSR = 0xF;	//	OSRVAL = 15
			UART0->BRG = 194;	// BRGVAL = 194 y OSRVAL = 15 (default) => BAUDRATE = FCLK / ((OSRVAL+1)*(BRGVAL+1)) = 9615
			UART0->INTENCLR = (1 << 0);	//	Disables RX interrupts
			UART0->INTENSET = (1 << 0);	//	Enables RX interrupts
			NVIC->ISER[0] = (1 << USART0_IRQn);	//	Enables NVIC interrupt
			UART0->CFG |= (1 << 0);	//	Enables UART0
			break;

		case 1:
			SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 15));	//	Enables clk for SWM, UART1
			//	Reset UART1
			SYSCON->PRESETCTRL0 &= ~(1 << 15);	//	RESET UART1
			SYSCON->PRESETCTRL0 |= (1 << 15);	//	RELEASE RESET UART1
			//	Assign UART pins  (PIO0_14 RXD, PIO0_15 TXD)
			SWM->PINASSIGN[1] &= ~((0xFF << 8) | (0xFF << 16));
			SWM->PINASSIGN[1] |= ((15 << 8) | (14 << 16));	//	Assigns RXD and TXD
			//	Config UART1
			UART1->CFG = (0 << 0);	//	Disables UART1
			UART1->CFG |= ((1 << 2) | (0 << 4) | (0 << 6) | (0 << 9) | (0 << 11) | (0 << 15));	//	8-bit, No-parity, 1-stopBit, No-flowControl, Asincronic, No-loop
			UART1->OSR = 12;	//	OSRVAL = 12
			UART1->BRG = 19;	// BRGVAL = 19 y OSRVAL = 12 => BAUDRATE = FCLK / ((OSRVAL+1)*(BRGVAL+1)) = 115384 ~ 115200
			UART1->INTENCLR = (1 << 0);	//	Disables RX interrupts
			UART1->INTENSET = (1 << 0);	//	Enables RX interrupts
			NVIC->ISER[0] = (1 << USART1_IRQn);	//	Enables NVIC interrupt
			UART1->CFG |= (1 << 0);	//	Enables UART1
			break;

		case 2:
			SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 16));	//	Enables clk for SWM, UART2
			//	Reset UART2
			SYSCON->PRESETCTRL0 &= ~(1 << 16);	//	RESET UART2
			SYSCON->PRESETCTRL0 |= (1 << 16);	//	RELEASE RESET UART2
			break;

		case 3:
			SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 30));	//	Enables clk for SWM, UART3
			//	Reset UART3
			SYSCON->PRESETCTRL0 &= ~(1 << 30);	//	RESET UART3
			SYSCON->PRESETCTRL0 |= (1 << 30);	//	RELEASE RESET UART3
			break;

		case 4:
			SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 31));	//	Enables clk for SWM, UART4
			//	Reset UART4
			SYSCON->PRESETCTRL0 &= ~(1 << 31);	//	RESET UART4
			SYSCON->PRESETCTRL0 |= (1 << 31);	//	RELEASE RESET UART4
			break;

		default:

			//	ERROR
			break;
	}
}

void Uart::pushRx(uint8_t data){	//	ISR pushes data into rx buffer
	uint32_t nextIndex = (m_tailRxIndex + 1) % m_maxRxLen;

	if(nextIndex == m_headRxIndex){	//	When msg >> TxBuffer, Tx Interrupts cant empty faster than pushTx loads => Overrun
			return;
	}else{
		m_bufferRx[m_tailRxIndex] = data;
		m_tailRxIndex++;
		m_tailRxIndex %= m_maxRxLen;	//	Resets when reaches [m_maxRxLen] value (circular buffer)
	}
}

bool Uart::popRx(uint8_t *data){	//	App pops data from rx buffer
	if(m_tailRxIndex != m_headRxIndex){	//	If the "tail" didnt catch the "head" => It can keep popping data
		*data = m_bufferRx[m_headRxIndex];	//	Read buffer
		m_headRxIndex++;
		m_headRxIndex %= m_maxRxLen;	//	Resets when reaches [m_maxRxLen] value (circular buffer)
		return true;
	}
	return false;
}

bool Uart::pushTx(uint8_t data){	//	App pushes data into tx buffer
	uint32_t nextIndex = (m_tailTxIndex + 1) % m_maxTxLen;

	if(nextIndex == m_headTxIndex){	//	When msg >> TxBuffer, Tx Interrupts cant empty faster than pushTx loads => Overrun
		return false;
	}else{
		m_bufferTx[m_tailTxIndex] = data;
		m_tailTxIndex++;
		m_tailTxIndex %= m_maxTxLen;	//	Resets when reaches [m_maxRxLen] value (circular buffer)
		return true;		//	m_headTxIndex is modified by ISR, so when Tx Interrupt reads, we can push in Tx Buffer
	}
}

bool Uart::popTx(uint8_t *data){	//	ISR pops data from tx buffer
	if(m_tailTxIndex != m_headTxIndex){	//	If the "tail" didnt catch the "head" => It can keep popping data
		*data = m_bufferTx[m_headTxIndex];	//	Reads buffer
		m_headTxIndex++;
		m_headTxIndex %= m_maxTxLen;	//	Resets when reaches [m_maxRxLen] value (circular buffer)
		return true;
	}
	return false;
}

bool Uart::sendStr(const char *msg){
	if(msg != nullptr && m_sendPtr == nullptr) m_sendPtr = msg;

	if(!m_sendPtr) return true;

	if(*m_sendPtr){
		if(Uart::pushTx(*m_sendPtr)){	//	Pushes one char into Tx buffer
			m_sendPtr++;				//	Next string position
		}
	}else{
		m_sendPtr = nullptr;
		return true;
	}

	if(!m_flagTx){
		m_flagTx = true;	//	Writing Tx buffer
		m_uart->INTENSET = (1 << 2);	//	Enable Tx Interrupt
	}
	return false;
}

uint8_t Uart::readStr(char *msg, uint32_t maxLen){
	uint8_t data = 0;

	if(Uart::popRx(&data)){
		if(m_readIndex < maxLen - 1){
			msg[m_readIndex++] = data;	//	Writes read buffer
		}

		if((m_readIndex >= maxLen - 1) || (data == 0)){
			data = m_readIndex;
			m_readIndex = 0;
			return data;
		}
	}
	return 0;
}

void Uart::isrHandler(){
	uint32_t stat = m_uart->STAT;
	uint8_t data;
	bool f_txSuccess;

	if(stat & (1 << 0)){	//	RXRDY
		data = ( uint8_t )m_uart->RXDAT;
		Uart::pushRx(data);	//	Saves RXDAT in rx buffer so it can be read later
	}
	if(stat & (1 << 2)){	//	TXRDY
		f_txSuccess = Uart::popTx(&data);	//	Reads tx buffer and loads data with its content

		if(f_txSuccess){
			m_uart->TXDAT = data;	//	Sends data
		}else{	//	Tried to read faster than wrote
			m_uart->INTENCLR = (1 << 2);	//	Disables Tx Interrupt
			m_flagTx = false;
		}
	}
}

void UART0_IRQHandler(void){
	if(uartInstance[0]) uartInstance[0]->isrHandler();
}

void UART1_IRQHandler(void){
	if(uartInstance[1]) uartInstance[1]->isrHandler();
}

void UART2_IRQHandler(void){
	if(uartInstance[2]) uartInstance[2]->isrHandler();
}

Uart::~Uart(){}

