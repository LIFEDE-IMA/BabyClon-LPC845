/*
 * spi.cpp
 *
 *  Created on: 18 jun. 2026
 *      Author: Julián - LIFEDE - UTN FRBA
 *      Consultas: jgross@frba.utn.edu.ar
 */

#include "spi.h"
#include "swm.h"

// Defines para funciones:

// la mascara para solo cambiar los bits de configuración global
#define SPI_CFG_CONFIGURATION_MASK (0b111101)

#define SPI_TXRDY_MASK (1 << 1)
#define SPI_RXRDY_MASK (1 << 0)

#define SPI_SSEL_MASK (0xF0000)
#define SPI_TXDATCTL_SSEL_ACTIVE(x) ((~(1 << (16 + (x)))) & SPI_SSEL_MASK)
#define SPI_TXDATCTL_DATA_LENGHT(x) ((x-1) << 24)

#define SLAVE_NO_SLAVE (0xF) // es para limpiar el registro con la funcion de seleccionar el slave, no se si se va a usar pero queda implementado
#define SLAVE_MAX_QUANTITY(x) ((x==SPI_NUMBER_0)? 4:2)

SPI_Type*  SPIs[]={ SPI0, SPI1 };

Spi *g_spi[ 2 ];

// ######################
// #                    #
// #    Parte 1: SPI    #
// #                    #
// ######################

Spi::Spi( 		bool portMOSI , uint8_t pinMOSI ,
				bool portMISO , uint8_t pinMISO ,
				bool portSCK  , uint8_t pinSCK  ,
				SPI_NUMBER N_spi,
				uint32_t clk_freq,
				uint32_t max_packets,
				SPI_CFG_BIT_ORDER_t spi_BITO,
				SPI_CFG_CPHA_t 		spi_CPHA,
				SPI_CFG_CPOL_t		spi_CPOL
			) : m_max_packets(max_packets), m_spi_n(N_spi)
{
	uint8_t iser ;
	m_spi = SPIs[m_spi_n];					// SPI seleccionada

	spi_packets = new SPI_packet[ max_packets ];	// buffer de packets
	m_inx_packetIn = m_inx_packetOut = 0;			// indices del bufer de packets
	m_flagTx = false ;					// flag de fin de Tx

	m_slave_count = 0;

	index_TX = 0;
	index_RX = 0;

	current_packet.TX_data = nullptr;		//	Agregado por Mati3 (31/07/26)
	current_packet.RX_data = nullptr;		//	Bug de hardFault al crear el primer paquete
	current_packet.done_flag = nullptr;
	current_packet.n_bytes = 0;
	current_packet.slave = 0;

	SYSCON->SYSAHBCLKCTRL0 |= (1 << 7); // Habilito la switch matrix. 7 = SWM

	switch ( m_spi_n )
	{
		case 0:
			g_spi[ 0 ] = this ;
			iser = SPI0_IRQn ;

			// 1: activar clock
			SYSCON->SYSAHBCLKCTRL0 |= ( 1 << 11 );

			// 2: reset de SPI (lo pide la datasheet)
			SYSCON->PRESETCTRL0 &= ~(1 << 11);
			SYSCON->PRESETCTRL0 |= (1 << 11);

			// el 3 se hace abajo, no inporta el orden (?

			// 4: configurar sw matrix
			PINASSIGN_Config(PA_SPI0_SCK,  portSCK,  pinSCK );
			PINASSIGN_Config(PA_SPI0_MOSI, portMOSI, pinMOSI);
			PINASSIGN_Config(PA_SPI0_MISO, portMISO, pinMISO);

			// 5: habilitación de clock
			SYSCON->FCLKSEL[ 9 ] = 1;
			break;
		case 1:
			g_spi[ 1 ] = this ;
			iser = SPI1_IRQn ;

			// 1: activar clock
			SYSCON->SYSAHBCLKCTRL0 |= ( 1 << 12 );

			// 2: reset de SPI (lo pide la datasheet)
			SYSCON->PRESETCTRL0 &= ~(1 << 12);
			SYSCON->PRESETCTRL0 |= (1 << 12);

			// el 3 se hace abajo, no inporta el orden (?

			// 4: configurar sw matrix
			PINASSIGN_Config(PA_SPI1_SCK,  portSCK,  pinSCK );
			PINASSIGN_Config(PA_SPI1_MOSI, portMOSI, pinMOSI);
			PINASSIGN_Config(PA_SPI1_MISO, portMISO, pinMISO);

			// 5: habilitación de clock
			SYSCON->FCLKSEL[ 10 ] = 1;

			break;
	}

	SYSCON->SYSAHBCLKCTRL0 &= ~(1 << 7); // desabilito la switch matrix. 7 = SWM


	// configuracion de la SPI


	m_spi->CFG =
			( 0 << 0 )				// 0= DISABLE 1=ENABLE
			| ( 1 << 2 )			// 0= Slave 1= Master
			| ( spi_BITO << 3 )		// 0= MSB first 1=LSB first
			| ( spi_CPHA << 4 )		// 0= Change. The SPI captures serial data on the first clock transition of the transfer (when the clock changes away from the rest state). Data is changed on the following edge. 1=  Capture. The SPI changes serial data on the first clock transition of the transfer (when the clock changes away from the rest state). Data is captured on the following edge.
			| ( spi_CPOL << 5 );	// 0= The rest state of the clock (between transfers) is low  1= The rest state of the clock (between transfers) is high.

	m_spi->DIV = ( FREQ_CLOCK / clk_freq ) - 1;

	m_spi->INTENSET |= ( 1 << 0 );		// RX interrupcion


	m_spi->TXCTL |= ((0x7) << 24); // estableciendo una trama de  bits (es el valor + 1):

	//3: enable del nvic
	NVIC->ISER[0] |= ( 1 << iser ); 			// habilitamos SPI_IRQ

	m_spi->CFG |= 	( 1 << 0 );			// habilitamos SPI
}

Spi::~Spi() {
	Tx_DisableInterupt();

	for(uint32_t i = 0; i < m_max_packets; i++){
		if(spi_packets[i].TX_data != nullptr){	//	Agregado por Mati3 (04/08/26)
			delete[] spi_packets[i].TX_data;	//	Solo para no tener pérdidas de memoria
			spi_packets[i].TX_data = nullptr;	//	Al destruir SPI con paquetes pendientes
		}
	}

	delete[] spi_packets;
	spi_packets = nullptr;
}

void Spi::push_packet ( SPI_packet packet )
{
	spi_packets[ m_inx_packetIn ] = packet;
	m_inx_packetIn ++;
	m_inx_packetIn %= m_max_packets;
}

uint8_t Spi::pop_packet (SPI_packet * packet )
{
	if ( m_inx_packetIn != m_inx_packetOut )
	{
		/*
		if(packet->TX_data != nullptr)	//	Eliminado por Mati3 (31/07/26)
		{								//	Provocaba intento de eliminacion de
			delete[] packet->TX_data;	//	Memoria inválida, lo dejo comentado por las dudas
		}
		*/
		*packet = spi_packets[ m_inx_packetOut ] ;

		spi_packets[m_inx_packetOut].TX_data = nullptr;		//	Agregado por Mati3 (04/08/26)
		spi_packets[m_inx_packetOut].RX_data = nullptr;		//	Limpieza de punteros
		spi_packets[m_inx_packetOut].done_flag = nullptr;	//	No era un bug pero ayuda a
		spi_packets[m_inx_packetOut].n_bytes = 0;			//	Encontrar los dueños de ciertos punteros

		m_inx_packetOut ++;
		m_inx_packetOut %= m_max_packets;
		index_TX = 0;
		index_RX = 0;
		return 1;
	}
	return 0;
}

void Spi::SPI_IRQHandler ( void )
{
	uint8_t dato = 0;
	uint32_t stat = m_spi->STAT;
	if( stat & SPI_TXRDY_MASK) // TXRDY
	{
		if( index_TX <  current_packet.n_bytes)
		{
			if(current_packet.TX_data != nullptr)
			{
				dato = *(current_packet.TX_data + index_TX);
			}
			m_spi->TXDATCTL = (
					SPI_TXDATCTL_SSEL_ACTIVE(current_packet.slave)		// SSELx
                |	SPI_TXDATCTL_DATA_LENGHT(8)							// estableciendo una trama de  bits (es el valor + 1):
                |	(dato)
			);

			if(index_TX == (current_packet.n_bytes - 1)) // es la ultima trama del paquete
			{
				m_spi->TXDATCTL |=	(1 << 20); // EOT = End of transfer
			}

			index_TX ++;
		}
		else
		{
			Tx_DisableInterupt ( );
		}
	}

	if( (stat & SPI_RXRDY_MASK) && m_flagTx ) // RXRDY
	{
		dato = ( uint8_t ) ((m_spi->RXDAT) & 0xFF);
		if(index_RX <  current_packet.n_bytes)
		{
			if(current_packet.RX_data != nullptr)
			{
				*(current_packet.RX_data + index_RX) = dato;
			}
			index_RX ++;
		}
	}

	if((index_TX == current_packet.n_bytes) && (index_RX == current_packet.n_bytes))
	{
		if (current_packet.done_flag != nullptr)
		{
			*(current_packet.done_flag) = true;
		}
		if(current_packet.TX_data != nullptr){	//	Agregado por Mati3 (31/07/26)
			delete[] current_packet.TX_data;	//	Solucion de bug hardFault
			current_packet.TX_data = nullptr;	//	Elimina paquete obsoleto
		}


		if(pop_packet(&current_packet) == 1)
		{
			Tx_EnableInterupt();
			m_flagTx = true ;
		}
		else
		{
			m_flagTx = false;
		}
	}

}

void Spi::Tx_EnableInterupt (  void )
{
	m_spi->INTENSET = (1 << 1);
}

void Spi::Tx_DisableInterupt (  void )
{
	m_spi->INTENCLR = (1 << 1);
}

void Spi::Transmit ( void * write_buff ,void * read_buff , uint32_t n, uint8_t slave_n ,  volatile bool* done)
{
	SPI_packet new_packet;

	if(write_buff != nullptr)
	{
		uint8_t* new_write_buff = (new uint8_t[n]);
		for (uint32_t i = 0; i < n; i++)
		{
			new_write_buff [i] = ((uint8_t*)write_buff)[i];
		}
		new_packet.TX_data = new_write_buff;
	}
	else
	{
		new_packet.TX_data = nullptr;
	}

	new_packet.done_flag = done;
	new_packet.n_bytes = n;
	new_packet.RX_data = (uint8_t*) read_buff;
	new_packet.slave = slave_n;

	if(done != nullptr)
	{
		*done = false;
	}

	push_packet(new_packet);

	if ( m_flagTx == false )
	{
		if(pop_packet(&current_packet) == 1){	//	Agregado por Mati3 (31/07/26) por bug HardFault al iniciar transmit
			m_flagTx = true ;					//	Estas 2 lineas ya estaban
			Tx_EnableInterupt (  );				//	Pero sin el if(pop_packet)
		}
	}
}


#define SLAVE_SWM_ARRAY {{PA_SPI0_SSEL0, PA_SPI0_SSEL1, PA_SPI0_SSEL2, PA_SPI0_SSEL3}, {PA_SPI1_SSEL0, PA_SPI1_SSEL1, PA_SPI1_SSEL1, PA_SPI1_SSEL1}} // repito el 1 1 por si llega a caer en ese caso, que la switch matrix no cambie algo importante


int8_t Spi::AddSlave(	bool portSS , uint8_t pinSS , SPI_SLAVE_SELECT_POLARITY_t polarity )
{
	if(m_slave_count < SLAVE_MAX_QUANTITY(m_spi_n))
	{
		const int8_t g_slave_swm_array[2][4] = SLAVE_SWM_ARRAY;

		if(polarity == SPI_SLAVE_SELECT_ACTIVE_HIGH)
		{
			m_spi->CFG |=  1 << (8 + m_slave_count); // polaridad del slave
		}

		SYSCON->SYSAHBCLKCTRL0 |= (1 << 7); // Habilito la switch matrix. 7 = SWM
		PINASSIGN_Config(g_slave_swm_array[m_spi_n][m_slave_count],  portSS,  pinSS);
		SYSCON->SYSAHBCLKCTRL0 &= ~(1 << 7); // desabilito la switch matrix. 7 = SWM

		m_slave_count++;
		return m_slave_count - 1;
	}
	else
	{
		return -1;
	}
}


void SPI0_IRQHandler ( void )
{
	g_spi[ 0 ]->SPI_IRQHandler();
}
void SPI1_IRQHandler ( void )
{
	g_spi[ 1 ]->SPI_IRQHandler();
}


// ############################
// #                          #
// #    Parte 2: SPI slave    #
// #                          #
// ############################



SpiSlave::SpiSlave(	bool portSS , uint8_t pinSS ,
					Spi &SPI_obj,
					Spi::SPI_SLAVE_SELECT_POLARITY_t polarity
				) :  m_portSS(portSS),   m_pinSS(pinSS),   m_SPI_obj(SPI_obj),   m_polarity(polarity)
{

	m_slave_number = SPI_obj.AddSlave(m_portSS , m_pinSS, m_polarity);
	m_spi_ref = &m_SPI_obj;
}

/*
# Funcion central de la clase
#### write_buff (void*):
Es el puntero a la dirección del buffer con la información, puede modificarse luego de llamar la función
#### read_buff (void*):
Es el puntero a la direccion del bufer de salida, que debe estar instanciado previamente. En caso de no necesitar lectura, dejar en nullptr
#### n (uint32_t):
Cantidad de tramas de trasmisión
#### done (bool*):
Flag que se establece en true una vez terminada la comunicación referida a ese paquete
*/
void SpiSlave::Transmit( void * write_buff ,void * read_buff , uint32_t n , volatile bool* done)
{
	m_spi_ref->Transmit(write_buff, read_buff, n, m_slave_number, done);
}
