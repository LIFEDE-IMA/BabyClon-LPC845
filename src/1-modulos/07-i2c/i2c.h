/*
 * i2c.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle MLX90614 temp sensor with the LPC845 I2C peripheral
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

#define TOTAL_I2C_SOURCES 4

#define RECEIVE_RDY	 0x1
#define TRANSMIT_RDY 0x2
#define NACK_ADDR	 0x3
#define NACK_DATA	 0x4

class I2C{
	private:
		I2C_Type* m_i2c;

		volatile uint8_t m_i2cState;
		volatile uint32_t m_mstState;
		volatile uint16_t m_data;
		volatile bool m_dataRdy;

		uint8_t m_slvAddr;		//	Each I2C instance has its own slave
		uint8_t m_slvRegAddr;	//	Each I2C instance has its own slave register to be read

		void init(uint8_t i2c);				//	Initializes I2C peripheral
		void enableNVIC_int(uint8_t i2c);	//	Enables NVIC Interrupt
		void enableMasterInt();				//	Enables Master Pending Interrupt
		void disableMasterInt();			//	Disables Master Pending Interrupt

		void isrHandler();	//	I2Cx ISR Handler

	public:
		enum i2c_states {
			IDLE,
			SEND_ADDR_W,
			SEND_REG,
			REPEATED_START,
			READ_LOW,
			READ_HIGH,
			STOP
		};

		I2C(uint8_t i2c);		//	Constructor

		void startReading(uint8_t slvAddress, uint8_t slvRegAddress);	//	Starts communication (master mode)
		bool isDataRdy();		//	Returns true if m_data is ready to be read
		uint16_t getData();		//	Returns m_data

		friend void I2C0_IRQHandler(void);	//	ISR Handler for I2C0
		friend void I2C1_IRQHandler(void);	//	ISR Handler for I2C1
		friend void I2C2_IRQHandler(void);	//	ISR Handler for I2C2
		friend void I2C3_IRQHandler(void);	//	ISR Handler for I2C3

		~I2C();					//	Destructor
};

#endif /* I2C_H_ */
