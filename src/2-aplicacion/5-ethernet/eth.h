/*
 * eth.h
 *
 *  Created on: 17 jul. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This class was written to handle W5500 Ethernet module with SPI peripheral
 */

#ifndef ETH_H_
#define ETH_H_

#include "spi.h"

#define MAX_SPI_TRANSFER_LEN	32

class Eth : public SpiSlave{
	public:
		enum block_t : uint8_t{
			COMMON_REGISTER_BLOCK = 0x00,	//	W5500 Common Register Block
			SOCKET0_REGISTER_BLOCK = 0x01,	//	W5500 Socket 0 Register Block
			SOCKET0_TX_BUFFER_BLOCK	= 0x02,	//	W5500 Socket 0 Tx Buffer Block
			SOKCET0_RX_BUFFER_BLOCK	= 0x03	//	W5500 Socket 0 Rx Buffer Block
		};

		enum registerAddr_t : uint16_t{
			MR_REGISTER = 0x0000,		//	W5500 Mode Register (Common Register Block)

			GAR0_REGISTER = 0x0001,		//	W5500 Gateway Address 0 Register (Common Register Block)
			GAR1_REGISTER = 0x0002,		//	W5500 Gateway Address 1 Register (Common Register Block)
			GAR2_REGISTER = 0x0003,		//	W5500 Gateway Address 2 Register (Common Register Block)
			GAR3_REGISTER = 0x0004,		//	W5500 Gateway Address 3 Register (Common Register Block)

			SUBR0_REGISTER = 0x0005,	//	W5500 Subnet Mask Address 0 Register (Common Register Block)
			SUBR1_REGISTER = 0x0006,	//	W5500 Subnet Mask Address 1 Register (Common Register Block)
			SUBR2_REGISTER = 0x0007,	//	W5500 Subnet Mask Address 2 Register (Common Register Block)
			SUBR3_REGISTER = 0x0008,	//	W5500 Subnet Mask Address 3 Register (Common Register Block)

			SHAR0_REGISTER = 0x0009,	//	W5500 Source Hardware Address 0 Register (Common Register Block)
			SHAR1_REGISTER = 0x000A,	//	W5500 Source Hardware Address 1 Register (Common Register Block)
			SHAR2_REGISTER = 0x000B,	//	W5500 Source Hardware Address 2 Register (Common Register Block)
			SHAR3_REGISTER = 0x000C,	//	W5500 Source Hardware Address 3 Register (Common Register Block)
			SHAR4_REGISTER = 0x000D,	//	W5500 Source Hardware Address 4 Register (Common Register Block)
			SHAR5_REGISTER = 0x000E,	//	W5500 Source Hardware Address 5 Register (Common Register Block)

			SIPR0_REGISTER = 0x000F,	//	W5500 Source IP Address 0 Register (Common Register Block)
			SIPR1_REGISTER = 0x0010,	//	W5500 Source IP Address 1 Register (Common Register Block)
			SIPR2_REGISTER = 0x0011,	//	W5500 Source IP Address 2 Register (Common Register Block)
			SIPR3_REGISTER = 0x0012,	//	W5500 Source IP Address 3 Register (Common Register Block)

			Sn_MR_REGISTER = 0x0000,		//	W5500 Socket n Mode Register (Socket Register Block)
			Sn_CR_REGISTER = 0x0001,		//	W5500 Socket n Command Register (Socket Register Block)
			Sn_IR_REGISTER = 0x0002,		//	W5500 Socket n Interrupt Register (Socket Register Block)
			Sn_SR_REGISTER = 0x0003,		//	W5500 Socket n Status Register (Socket Register Block)

			Sn_PORT0_REGISTER = 0x0004,		//	W5500 Socket n Source Port 0 (Socket Register Block)
			Sn_PORT1_REGISTER = 0x0005,		//	W5500 Socket n Source Port 1 (Socket Register Block)

			Sn_DHAR0_REGISTER = 0x0006,		//	W5500 Socket n Destination Hardware Address 0 Register (Socket Register Block)
			Sn_DHAR1_REGISTER = 0x0007,		//	W5500 Socket n Destination Hardware Address 1 Register (Socket Register Block)
			Sn_DHAR2_REGISTER = 0x0008,		//	W5500 Socket n Destination Hardware Address 2 Register (Socket Register Block)
			Sn_DHAR3_REGISTER = 0x0009,		//	W5500 Socket n Destination Hardware Address 3 Register (Socket Register Block)
			Sn_DHAR4_REGISTER = 0x000A,		//	W5500 Socket n Destination Hardware Address 4 Register (Socket Register Block)
			Sn_DHAR5_REGISTER = 0x000B,		//	W5500 Socket n Destination Hardware Address 5 Register (Socket Register Block)

			Sn_DIPR0_REGISTER = 0x000C,		//	W5500 Socket n Destination IP Address 0 Register (Socket Register Block)
			Sn_DIPR1_REGISTER = 0x000D,		//	W5500 Socket n Destination IP Address 1 Register (Socket Register Block)
			Sn_DIPR2_REGISTER = 0x000E,		//	W5500 Socket n Destination IP Address 2 Register (Socket Register Block)
			Sn_DIPR3_REGISTER = 0x000F,		//	W5500 Socket n Destination IP Address 3 Register (Socket Register Block)

			Sn_DPORT0_REGISTER = 0x0010,	//	W5500 Socket n Destination Port 0 (Socket Register Block)
			Sn_DPORT1_REGISTER = 0x0011,	//	W5500 Socket n Destination Port 1 (Socket Register Block)

			VERSIONR_REGISTER = 0x0039	//	W5500 Chip Version Register (Common Register Block)
		};

		enum opMode_t : uint8_t{
			VAR_DATA_LEN = 0,
			FIXED_DATA_LEN_1,
			FIXED_DATA_LEN_2,
			FIXED_DATA_LEN_4
		};

		enum rwMode_t : uint8_t{
			READ = 0,
			WRITE
		};

		enum socketMode_t : uint8_t{
			TCP_MODE = 0x01,
			UDP_MODE = 0x02
		};

		enum socketCmd_t : uint8_t{
			OPEN_SOCKET = 0x01,
			LISTEN_SOCKET = 0x02,
			CONNECT_SOCKET = 0x04,
			DISCONNECT_SOCKET = 0x08,
			CLOSE_SOCKET = 0x10,
			SEND_SOCKET = 0x20,
			RECV_SOCKET = 0x40
		};

		enum socketStat_t : uint8_t{
			SOCK_CLOSED = 0x00,
			SOCK_INIT = 0x13,
			SOCK_LISTEN = 0x14,
			SOCK_ESTABLISHED = 0x17,
			SOCK_CLOSE_WAIT = 0x1C
		};

		enum ethState_t{
			ETH_IDLE,
			ETH_SOCKET_WRITE_MODE,
			ETH_SOCKET_WAIT_WRITE_MODE,
			ETH_SOCKET_WRITE_PORT,
			ETH_SOCKET_WAIT_WRITE_PORT,
			ETH_SOCKET_WRITE_COMMAND,
			ETH_SOCKET_WAIT_WRITE_COMMAND,
			ETH_SOCKET_FINISHED
		};

	private:
		uint8_t m_txBuffer[MAX_SPI_TRANSFER_LEN + 3];
		uint8_t m_rxBuffer[MAX_SPI_TRANSFER_LEN + 3];

		uint8_t *m_readBuffer;
		uint16_t m_readLen;

		volatile bool *m_doneFlag;
		bool m_pendingReadFlag;

		//	SOCKETS
		ethState_t m_ethState;
		uint8_t m_socketPortBuffer[2];
		volatile bool m_socketTransferDone;

		void transfer(registerAddr_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);		//	Makes a transfer (r/w) with W5500

		void readByte(registerAddr_t addr, block_t block, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Reads ONE BYTE from W5500 register
		uint8_t readByteAndWait(registerAddr_t addr, block_t block, opMode_t opMode = opMode_t::VAR_DATA_LEN);								//	Blocking version for debug
		void writeByte(registerAddr_t addr, block_t block, uint8_t data, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Writes ONE BYTE into W5500 register

	public:
		Eth(bool portCS, uint8_t pinCS, Spi &spi);	//	Constructor

		void readBuffer(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Reads [len] bytes from W5500 register and saves them in user [buffer]
		void readBufferAndWait(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug
		void writeBuffer(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Writes [len] bytes into W5500 register from user [buffer]
		void writeBufferAndWait(registerAddr_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug

		void setIP(uint8_t ip[4], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);				//	Sets SIPRn Registers in W5500
		void setIPAndWait(uint8_t ip[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);								//	Blocking version for debug
		void readIP(uint8_t ip[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);									//	Reads SIPRn Registers in W5500

		void setGateway(uint8_t gateway[4], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Sets GARn Registers in W5500
		void setGatewayAndWait(uint8_t gateway[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug
		void readGateway(uint8_t gateway[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);							//	Reads GARn Registers in W5500

		void setSubnet(uint8_t subnet[4], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);		//	Sets SUBRn Registers in W5500
		void setSubnetAndWait(uint8_t subnet[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);						//	Blocking version for debug
		void readSubnet(uint8_t subnet[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);							//	Reads SUBRn Registers in W5500

		void setMAC(uint8_t mac[6], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);			//	Sets SHARn Registers in W5500
		void setMACAndWait(uint8_t mac[6], opMode_t opMode = opMode_t::VAR_DATA_LEN);							//	Blocking version for debug
		void readMAC(uint8_t mac[6], opMode_t opMode = opMode_t::VAR_DATA_LEN);									//	Reads SHARn Registers in W5500

		void init();	//	Initializes W5500 Module

		void socketOpenTCP(uint16_t port);					//	Opens Socket, TCP Socket Mode
		void socketClose();									//	Closes Socket
		socketStat_t socketStatus();						//	Returns Socket Status
		void socketConnect(uint8_t ip[4], uint16_t port);	//	Connects Socket
		bool socketConnected();								//	Returns True if Socket is Connected

		void handler();		//	Non-blocking W5500 handler

		~Eth();		//	Destructor
};

#endif /* ETH_H_ */
