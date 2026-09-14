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

class Uart{
	public:
		enum uart_t : uint8_t{
			UART_0 = 0,
			UART_1,
			UART_2,
			UART_3,
			UART_4
		};

		static const uint8_t TOTAL_UART_SOURCES = 5;

		static const uint8_t MAX_RX_LEN = 254;
		static const uint8_t MAX_TX_LEN = 254;

		static const int8_t NO_DATA_RECEIVED = -1;

	private:
		static const uint8_t RXRDY = (1 << 0);
		static const uint8_t TXRDY = (1 << 2);

		UART_Type* m_uart;

		bool m_rxPort;
		bool m_txPort;
		uint8_t m_rxPin;
		uint8_t m_txPin;

		uint32_t m_baudrate;
		uint8_t m_osr;

		uint8_t m_bufferRx[Uart::MAX_RX_LEN];
		uint32_t m_maxRxLen;
		volatile uint32_t m_headRxIndex;	//	Circular buffer
		volatile uint32_t m_tailRxIndex;	//	Circular buffer
		uint8_t m_bufferTx[Uart::MAX_TX_LEN];
		uint32_t m_maxTxLen;
		volatile uint32_t m_headTxIndex;	//	Circular buffer
		volatile uint32_t m_tailTxIndex;	//	Circular buffer
		volatile bool m_flagTx;				//	Filling / Not Filling Tx Buffer

		const char *m_sendPtr;
		uint8_t m_readIndex;

		bool m_headerByteSetFlag;	//	True if uart trama has a header byte
		bool m_footerByteSetFlag;	//	True if uart trama has a footer byte
		uint8_t m_headerByte;
		uint8_t m_footerByte;
		bool m_headerByteSentFlag;
		bool m_footerByteSentFlag;

		void initUart(uart_t uartNumber);	//	Initializes UARTx

		void disableRxInt(void);	//	Disables Rx Interrupts
		void enableRxInt(void);		//	Enables Rx Interrupts
		void disableTxInt(void);	//	Disables Tx Interrupts
		void enableTxInt(void);		//	Enables Tx Interrupts

		void pushRx(uint8_t data);	//	ISR pushes data into rx buffer
		bool popRx(uint8_t *data);	//	App pops data from rx buffer
		bool pushTx(uint8_t data);	//	App pushes data into tx buffer
		bool popTx(uint8_t *data);	//	ISR pops data from tx buffer

		void sendHeaderByte(void);
		void sendFooterByte(void);

		void isrHandler(void);		//	UARTx ISR handler

	public:
		Uart(uart_t uartNumber, bool rxPort, uint8_t rxPin, bool txPort, uint8_t txPin, uint32_t baudrate = 9600, uint8_t OSRval = 0xF);		//	Constructor

		void setHeaderByte(uint8_t headerByte);			//	Uart trama will now have a header byte
		void setFooterByte(uint8_t footerByte);			//	Uart trama will now have a footer byte

		int16_t receiveByte(void);						//	Returns byte received or -1 if no data was received

		bool sendStr(const char *msg);						//	Transmit string
		int16_t readStr(char *readBuff, uint32_t maxLen);	//	Read received string, returns received len or -1 if nothing was received

		friend void UART0_IRQHandler(void);	//	ISR Handler for	UART0
		friend void UART1_IRQHandler(void);	//	ISR Handler for	UART1
		friend void UART2_IRQHandler(void);	//	ISR Handler for	UART2

		~Uart();							//	Destructor
};


#endif /* UART_H_ */
