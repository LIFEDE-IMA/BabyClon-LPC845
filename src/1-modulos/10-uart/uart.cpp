#include "uart.h"

UART_Type* UARTS[] = {UART0, UART1, UART2, UART3, UART4};

Uart *uartInstance[Uart::TOTAL_UART_SOURCES] = {nullptr, nullptr, nullptr, nullptr, nullptr};

Uart::Uart(uart_t uartNumber, bool rxPort, uint8_t rxPin, bool txPort, uint8_t txPin, uint32_t baudrate, uint8_t OSRval){
	m_uart = UARTS[uartNumber];

	m_rxPort = rxPort;
	m_rxPin = rxPin;
	m_txPort = txPort;
	m_txPin = txPin;

	m_baudrate = baudrate;
	m_osr = OSRval;

	m_maxRxLen = Uart::MAX_RX_LEN;
	for(uint8_t i = 0; i < m_maxRxLen; i++) m_bufferRx[i] = 0;
	m_headRxIndex = 0;
	m_tailRxIndex = 0;
	m_maxTxLen = Uart::MAX_TX_LEN;
	for(uint8_t i = 0; i < m_maxTxLen; i++) m_bufferTx[i] = 0;
	m_headTxIndex = 0;
	m_tailTxIndex = 0;
	m_flagTx = false;
	m_sendPtr = nullptr;
	m_readIndex = 0;

	m_headerByteSetFlag = false;
	m_footerByteSetFlag = false;
	m_headerByte = 0;
	m_footerByte = 0;
	m_headerByteSentFlag = false;
	m_footerByteSentFlag = false;

	uartInstance[uartNumber] = this;

	Uart::initUart(uartNumber);
}

void Uart::initUart(uart_t uartNumber){	//	NVIC (Cap. 7), SYSCON (Cap. 8), SWM (Cap. 10), UART (Cap. 17)
	SYSCON->FCLKSEL[uartNumber] = 0x1;	//	Clk source for UARTx = main (30MHz)

	SYSCON->SYSAHBCLKCTRL0 |= (1 << 7);	//	Enable SWM clk

	uint8_t rxPIO = (m_rxPin + (m_rxPort * 32));
	uint8_t txPIO = (m_txPin + (m_txPort * 32));

	uint8_t nvicShift = 0;		//	Uart 0

	switch(uartNumber){
		case uart_t::UART_0:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 14);	//	Enable UART0 clk
			//	Reset UART0
			SYSCON->PRESETCTRL0 &= ~(1 << 14);	//	RESET UART0
			SYSCON->PRESETCTRL0 |= (1 << 14);	//	CLEAR RESET UART0
			//	Assign UART0 pins
			SWM->PINASSIGN[0] &= ~((0xFF << 0) | (0xFF << 8));
			SWM->PINASSIGN[0] |= ((txPIO << 0) | (rxPIO << 8));	//	Assign RXD and TXD
			nvicShift = 0;
			break;

		case uart_t::UART_1:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 15);	//	Enable UART1 clk
			//	Reset UART1
			SYSCON->PRESETCTRL0 &= ~(1 << 15);	//	RESET UART1
			SYSCON->PRESETCTRL0 |= (1 << 15);	//	CLEAR RESET UART1
			//	Assign UART1 pins
			SWM->PINASSIGN[1] &= ~((0xFF << 8) | (0xFF << 16));
			SWM->PINASSIGN[1] |= ((txPIO << 8) | (rxPIO << 16));	//	Assign RXD and TXD
			nvicShift = 1;
			break;

		case uart_t::UART_2:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 16);	//	Enable UART2 clk
			//	Reset UART2
			SYSCON->PRESETCTRL0 &= ~(1 << 16);	//	RESET UART2
			SYSCON->PRESETCTRL0 |= (1 << 16);	//	CLEAR RESET UART2
			//	Assign UART2 pins
			SWM->PINASSIGN[2] &= ~((0xFF << 16) | (0xFF << 24));
			SWM->PINASSIGN[2] |= ((txPIO << 16) | (rxPIO << 24));	//	Assign RXD and TXD
			nvicShift = 2;
			break;

		case uart_t::UART_3:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 30);	//	Enable UART3 clk
			//	Reset UART3
			SYSCON->PRESETCTRL0 &= ~(1 << 30);	//	RESET UART3
			SYSCON->PRESETCTRL0 |= (1 << 30);	//	CLEAR RESET UART3
			//	Assign UART3 pins
			SWM->PINASSIGN[11] &= ~(0xFF << 24);
			SWM->PINASSIGN[12] &= ~(0xFF << 0);
			SWM->PINASSIGN[11] |= (txPIO << 24);	//	Assign TXD
			SWM->PINASSIGN[12] |= (rxPIO << 0);		//	Assign RXD
			nvicShift = 27;
			break;

		case uart_t::UART_4:
			SYSCON->SYSAHBCLKCTRL0 |= (1 << 31);	//	Enable UART4 clk
			//	Reset UART4
			SYSCON->PRESETCTRL0 &= ~(1 << 31);	//	RESET UART4
			SYSCON->PRESETCTRL0 |= (1 << 31);	//	CLEAR RESET UART4
			//	Assign UART4 pins
			SWM->PINASSIGN[12] &= ~((0xFF << 16) | (0xFF << 24));
			SWM->PINASSIGN[12] |= ((txPIO << 16) | (rxPIO << 24));	//	Assign RXD and TXD
			nvicShift = 28;
			break;

		default:
			//	ERROR
			break;
	}

	//	UARTx config
	m_uart->CFG = (0 << 0);		//	Disable UARTx
	m_uart->CFG |= ((1 << 2)  |	//	8-bit
				    (0 << 4)  |	//	No-parity
				    (0 << 6)  |	//	1-stopBit
				    (0 << 9)  |	//	No-flowControl
				    (0 << 11) |	//	Asynchronous
				    (0 << 15));	//	No-loop
	m_uart->OSR = m_osr;	//	OSRVAL = 15
	m_uart->BRG = ((FREQ_CLOCK / (m_baudrate * (m_osr + 1))) - 1);	// BRGVAL = 194 y OSRVAL = 15 (default) => BAUDRATE = FCLK / ((OSRVAL+1)*(BRGVAL+1)) = 9615

	Uart::disableRxInt();	//	Deshabilita RX interrupts
	Uart::disableTxInt();	//	Deshabilita TX interrupts
	Uart::enableRxInt();	//	Habilita RX interrupts

	NVIC->ISER[0] = (1 << (USART0_IRQn + nvicShift));	//	Enable NVIC interrupt
	m_uart->CFG |= (1 << 0);	//	Enable UARTx
}

void Uart::disableRxInt(void){ m_uart->INTENCLR = (1 << 0); }

void Uart::enableRxInt(void){ m_uart->INTENSET = (1 << 0); }

void Uart::disableTxInt(void){ m_uart->INTENCLR = (1 << 2); }

void Uart::enableTxInt(void){ m_uart->INTENSET = (1 << 2); }

void Uart::pushRx(uint8_t data){	//	ISR pushes data into rx buffer
	uint32_t nextIndex = (m_tailRxIndex + 1) % m_maxRxLen;

	if(nextIndex == m_headRxIndex){	//	When msg >> RxBuffer, Rx Interrupts can load faster than popRx empties => Overrun
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

void Uart::setHeaderByte(uint8_t headerByte){
	m_headerByteSetFlag = true;
	m_headerByte = headerByte;
}

void Uart::setFooterByte(uint8_t footerByte){
	m_footerByteSetFlag = true;
	m_footerByte = footerByte;
}

void Uart::sendHeaderByte(void){
	if(Uart::pushTx(m_headerByte)){	//	Pushes header byte into Tx buffer
		if(!m_flagTx){
			m_flagTx = true;	//	Writing Tx buffer
			Uart::enableTxInt();
		}
	}
	m_headerByteSentFlag = true;
}

void Uart::sendFooterByte(void){
	if(Uart::pushTx(m_footerByte)){	//	Pushes footer byte into Tx buffer
		if(!m_flagTx){
			m_flagTx = true;	//	Writing Tx buffer
			Uart::enableTxInt();
		}
	}
	m_footerByteSentFlag = true;
}

int16_t Uart::receiveByte(void){
	uint8_t data = 0;

	if(Uart::popRx(&data)){
		return data;
	}
	return -1;
}

bool Uart::sendStr(const char *msg){
	if(msg != nullptr && m_sendPtr == nullptr)
		m_sendPtr = msg;

	if(!m_sendPtr)
		return true;

	if(m_headerByteSetFlag){
		if(!m_headerByteSentFlag){
			Uart::sendHeaderByte();
			return false;
		}
	}

	if(*m_sendPtr){
		if(Uart::pushTx(*m_sendPtr)){	//	Pushes one char into Tx buffer
			m_sendPtr++;				//	Next string position
		}
	}else{	//	Finished string (*m_sendPtr = '\0')
		if(m_footerByteSetFlag){
			if(!m_footerByteSentFlag){
				Uart::sendFooterByte();
				return false;
			}
		}
		m_sendPtr = nullptr;
		m_headerByteSentFlag = false;
		m_footerByteSentFlag = false;
		return true;
	}

	if(!m_flagTx){
		m_flagTx = true;	//	Writing Tx buffer
		Uart::enableTxInt();
	}

	return false;
}

int16_t Uart::readStr(char *readBuff, uint32_t maxLen){
	uint8_t data = 0;

	if(Uart::popRx(&data)){
		if(m_readIndex < maxLen - 1){
			readBuff[m_readIndex++] = data;	//	Writes read buffer
		}

		if((m_readIndex >= maxLen - 1) || (data == 0)){
			data = m_readIndex;
			m_readIndex = 0;
			return data;	//	Returns received len (if '\0' was before maxLen)
		}
	}
	return -1;	//	Nothing was received, return -1
}

void Uart::isrHandler(){
	uint32_t stat = m_uart->STAT;
	uint8_t data;
	bool f_txSuccess;

	if(stat & Uart::RXRDY){	//	RXRDY
		data = (uint8_t)m_uart->RXDAT;
		Uart::pushRx(data);	//	Saves RXDAT in rx buffer so it can be read later
	}
	if(stat & Uart::TXRDY){	//	TXRDY
		f_txSuccess = Uart::popTx(&data);	//	Reads tx buffer and loads data with its content

		if(f_txSuccess){
			m_uart->TXDAT = data;	//	Sends data
		}else{	//	Tried to read faster than wrote
			Uart::disableTxInt();
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

