/*
 * i2c.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle MLX90614 temp sensor with the LPC845 I2C peripheral
 *
 *	Transfer() op description:
 *
 *	[START] ~ addr+W ~ tx[0...txLen-1] ~ [REPEATED START] ~ addr+R ~ rx[0...rxLen-1] ~ [STOP]
 *
 *	Combinations:
 *		#	txLen > 0, rxLen = 0 -> Plain Write
 *		#	txLen = 0, rxLen > 0 -> Plain Read
 *		#	txLen > 0, rxLen > 0 -> Write, Repeated Start, Read
 *		#	txLen = 0, rxLen = 0 -> Address Only (Bus Scan)
 */

#ifndef I2C_H_
#define I2C_H_

#include "LPC845.h"

#if defined (__cplusplus)
extern "C" {

void I2C0_IRQHandler(void);
void I2C1_IRQHandler(void);
void I2C2_IRQHandler(void);
void I2C3_IRQHandler(void);

}
#endif

class I2C{
	public:
		static const uint8_t TOTAL_I2C_SOURCES = 4;
		static const uint32_t MAX_FREQ_HZ = 400000;

		enum i2cStatus_t : uint8_t{
			BUS_IDLE = 0,
			BUS_BUSY,		//	Transfer in Progress
			BUS_OP_DONE,	//	Transfer Finished OK
			ERR_NACK_ADDR,	//	Slave Didnt Acknowledge Its Address
			ERR_NACK_DATA,	//	Slave Didnt Acknowledge a Data Byte
			ERR_ARB_LOST,	//	Arbitration Lost (Another Master in the Bus Took Control)
			ERR_BUS,		//	Start / Stop Error or Unexpected Hardware State
			ERR_TIMEOUT,	//	Timeout Expired
			ABORTED			//	abort() was Called
		};

	private:
		static const uint8_t STAT_MSTPENDING = 0x1;			//	STAT: 0
		static const uint8_t STAT_MST_IDLE = 0x0;			//	STAT: 3:1
		static const uint8_t STAT_MST_RX_RDY = 0x1;			//	STAT: 3:1
		static const uint8_t STAT_MST_TX_RDY = 0x2;			//	STAT: 3:1
		static const uint8_t STAT_MST_NACK_ADDR = 0x3;		//	STAT: 3:1
		static const uint8_t STAT_MST_NACK_DATA = 0x4;		//	STAT: 3:1
		static const uint8_t STAT_MSTARBLOSS = (1 << 4);	//	STAT: 4
		static const uint8_t STAT_MSTMSTPERR = (1 << 6);	//	STAT: 6 (Start / Stop Error)
		static const uint32_t STAT_EVENTTIMEOUT = (1 << 24);	//	STAT: 24
		static const uint32_t STAT_SCLTIMEOUT = (1 << 25);	//	STAT: 25

		static const uint8_t MSTCTL_CONTINUE = (1 << 0);//	MSTCTL: 0
		static const uint8_t MSTCTL_START = (1 << 1);	//	MSTCTL: 1
		static const uint8_t MSTCTL_STOP = (1 << 2);	//	MSTCTL: 2

		enum i2cStates_t : uint8_t {
			ST_IDLE,
			ST_TX,		//	Address + W
			ST_RX,		//	Address + R
			ST_STOP
		};

		I2C_Type* m_i2c;
		uint8_t m_i2cNumber;

		bool m_SDAport;
		uint8_t m_SDApin;
		bool m_SCLport;
		uint8_t m_SCLpin;

		uint32_t m_freqHz;		//	SCL Freq
		uint32_t m_baseClkHz;	//	Clk After CLKDIV

		//	Transfer Members
		volatile i2cStates_t m_i2cState;
		volatile i2cStatus_t m_i2cBusStatus;
		i2cStatus_t m_transferResult;
		uint8_t m_slvAddr;		//	7 bit address
		const uint8_t *m_txBuff;
		uint16_t m_txLen;
		volatile uint16_t m_txIdx;
		uint8_t *m_rxBuff;
		uint16_t m_rxLen;
		volatile uint16_t m_rxIdx;
		uint8_t m_slvRegByte;

		//	Timeout
		uint16_t m_timeoutMs;

		void init(void);				//	Initializes I2C peripheral

		void enableNVIC_int(void);		//	Enables NVIC Interrupt
		void disableNVIC_int(void);		//	Disables NVIC Interrupt
		void enableMasterInt(void);		//	Enables Master Pending Interrupt
		void disableMasterInt(void);	//	Disables Master Pending Interrupt

		void setTimeout(void);			//	Writes TIMEOUT Register

		void startStop(i2cStatus_t result);	//	Sends Stop and Reports [result]
		void finish(i2cStatus_t status);	//	Ends the Transfer
		void abort(i2cStatus_t status);		//	Reset Master

		void isrHandler();	//	I2Cx ISR Handler

	public:
		I2C(uint8_t i2cNumber, bool SDAport, uint8_t SDApin,  bool SCLport, uint8_t SCLpin, uint32_t freqHz = 100000, uint16_t timeoutMs = 30);		//	Constructor

		bool setFreq(uint32_t freqHz);	//	Can Only be Called when IDLE
		uint32_t getFreq(void) const;	//	Returns [m_freqHz]
		bool setTimeout(uint16_t ms);	//	0: disabled

		bool transfer(uint8_t slvAddress, const uint8_t *tx, uint16_t txLen, uint8_t *rx, uint16_t rxLen);

		bool write(uint8_t slvAddress, const uint8_t *tx, uint16_t txLen);
		bool read(uint8_t slvAddress, uint8_t *rx, uint16_t rxLen);
		bool readRegister(uint8_t slvAddress, uint8_t regAddress, uint8_t *rx, uint16_t rxLen);

		bool presence(uint8_t slvAddress);	//	True if Slave Answers

		bool isBusy(void) const;				//	Returns [m_i2cBusStatus == BUS_BUSY]
		bool isDone(void) const;				//	Returns [m_i2cBusStatus == BUS_OP_DONE]
		bool hasError(void) const;				//	Returns True if [m_i2cBusStatus == ERR_xxxx]
		I2C::i2cStatus_t getStatus(void) const;	//	Returns [m_i2cBusStatus]

		uint16_t getBytesWritten(void) const;	//	Of the Last / Current Transfer
		uint16_t getBytesRead(void) const;		//	Of the Last / Current Transfer

		void abort();	//	Cancels Current Transfer

		friend void I2C0_IRQHandler(void);	//	ISR Handler for I2C0
		friend void I2C1_IRQHandler(void);	//	ISR Handler for I2C1
		friend void I2C2_IRQHandler(void);	//	ISR Handler for I2C2
		friend void I2C3_IRQHandler(void);	//	ISR Handler for I2C3

		~I2C();					//	Destructor
};

#endif /* I2C_H_ */
