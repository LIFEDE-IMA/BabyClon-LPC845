/*
 * spi.h
 *
 *  Created on: 18 jun. 2026
 *      Author: Julián - LIFEDE - UTN FRBA
 *      Consultas: jgross@frba.utn.edu.ar
 */

#ifndef SPI_H_
#define SPI_H_

#include "LPC845.h"

#if defined (__cplusplus)
extern "C" {

void SPI0_IRQHandler ( void );
void SPI1_IRQHandler ( void );

}
#endif

#define SPI_DEFAULT_MAX_PACKETS (32)

typedef struct {
	uint8_t* TX_data;
	uint8_t* RX_data;
	uint32_t n_bytes;
	uint8_t slave;
	volatile bool* done_flag;
} SPI_packet;

// ############################
// #                          #
// #       Part 1: SPI		  #
// #                          #
// ############################

class SPI{
	private:
		SPI_Type *m_spi;

		SPI_packet *spi_packets;
		SPI_packet current_packet;
		uint32_t m_inx_packetIn, m_inx_packetOut, m_max_packets;

		uint32_t index_TX, index_RX;

		uint8_t m_slave_count;
		uint8_t m_spi_n;

		bool m_flagTx;

	public:
		//	LSB First Mode
		typedef enum {
			SPI_MODE_BIT_ORDER_MSB_F = 0,	// Standard. Data is transmitted and received in standard MSB first order
			SPI_MODE_BIT_ORDER_LSB_F = 1	// Reverse. Data is transmitted and received in reverse order (LSB first)
		} SPI_CFG_BIT_ORDER_t;

		// Clock Polarity Select
		typedef enum {
			SPI_CPHA_CHANGE = 0 ,			// The SPI captures serial data on the first clock transition of the transfer
			SPI_CPHA_CAPTURE = 1			// The SPI changes serial data on the first clock transition of the transfer
		} SPI_CFG_CPHA_t;

		// Clock Phase Select
		typedef enum {
			SPI_CPOL_LOW_CLOCK = 0 ,		// The rest state of the clock (between transfers) is low
			SPI_CPOL_HIGH_CLOCK = 1			// The rest state of the clock (between transfers) is high
		} SPI_CFG_CPOL_t;

		typedef enum {
			SPI_SLAVE_SELECT_ACTIVE_LOW = 0,	//  The SSEL0 pin is active low. The value in the SSEL fields of the RXDAT, TXDATCTL, and TXCTL registers related to SSEL is not inverted relative to the pins
			SPI_SLAVE_SELECT_ACTIVE_HIGH = 1	//  The SSEL0 pin is active high. The value in the SSEL fields of the RXDAT, TXDATCTL, and TXCTL registers related to SSEL is inverted relative to the pins.
		} SPI_SLAVE_SELECT_POLARITY_t;


		typedef enum {
			SPI_NUMBER_0 = 0,
			SPI_NUMBER_1 = 1
		} SPI_NUMBER;

		SPI( 	bool portMOSI , uint8_t pinMOSI ,
				bool portMISO , uint8_t pinMISO ,
				bool portSCK  , uint8_t pinSCK  ,
				SPI_NUMBER N_spi,
				uint32_t clk_freq,
				uint32_t max_packets = SPI_DEFAULT_MAX_PACKETS,
				SPI_CFG_BIT_ORDER_t spi_BITO = SPI_MODE_BIT_ORDER_MSB_F,
				SPI_CFG_CPHA_t 		spi_CPHA = SPI_CPHA_CHANGE,
				SPI_CFG_CPOL_t		spi_CPOL = SPI_CPOL_LOW_CLOCK
			);

		void Transmit ( void * write_buff ,void * read_buff , uint32_t n, uint8_t slave_n , volatile bool* done = nullptr);

		void Select_Slave(uint8_t slave_n, bool clear_others = true);

		void SPI_IRQHandler (void);
		int8_t AddSlave(bool portSS , uint8_t pinSS , SPI_SLAVE_SELECT_POLARITY_t polarity );

		~SPI();

	private:
		uint8_t pop_packet (SPI_packet * dato ) ;
		void push_packet ( SPI_packet dato ) ;

		void Tx_EnableInterupt (  void );
		void Tx_DisableInterupt (  void );
};

// ############################
// #                          #
// #     Part 2: SPI slave    #
// #                          #
// ############################

class SpiSlave{
	private:
		uint8_t m_slave_number;
		SPI* m_spi_ref;
		bool m_portSS;
		uint8_t m_pinSS;
		SPI &m_SPI_obj;
		SPI::SPI_SLAVE_SELECT_POLARITY_t m_polarity;

	public:
		SpiSlave(	bool portSS , uint8_t pinSS ,
					SPI &SPI_obj,
					SPI::SPI_SLAVE_SELECT_POLARITY_t polarity = SPI::SPI_SLAVE_SELECT_ACTIVE_LOW
				);

		~SpiSlave(){}

		void Transmit ( void * write_buff ,void * read_buff , uint32_t n , volatile bool* done = nullptr);
};

#endif /* SPI_H_ */
