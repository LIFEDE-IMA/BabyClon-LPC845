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
#include "systimer.h"

#define MAX_SPI_TRANSFER_LEN	128

class Eth : public SpiSlave{
	public:
		enum block_t : uint8_t{
			COMMON_REGISTER_BLOCK = 0x00,	//	W5500 Common Register Block
			SOCKET0_REGISTER_BLOCK = 0x01,	//	W5500 Socket 0 Register Block
			SOCKET0_TX_BUFFER_BLOCK	= 0x02,	//	W5500 Socket 0 Tx Buffer Block
			SOCKET0_RX_BUFFER_BLOCK	= 0x03	//	W5500 Socket 0 Rx Buffer Block
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

			Sn_RXBUF_SIZE_REGISTER = 0x001E,			//	W5500 Socket n Receive BufferSize (Socket Register Block)
			Sn_TXBUF_SIZE_REGISTER = 0x001F,			//	W5500 Socket n Transmit BufferSize (Socket Register Block)

			Sn_TX_FSR0_REGISTER = 0x0020,	//	W5500 Socket n Tx Free Size 0 Register (Socket Register Block)
			Sn_TX_FSR1_REGISTER = 0x0021,	//	W5500 Socket n Tx Free Size 1 Register (Socket Register Block)

			Sn_TX_RD0_REGISTER = 0x0022,	//	W5500 Socket n Tx Read Pointer 0 Register (Socket Register Block)
			Sn_TX_RD1_REGISTER = 0x0023,	//	W5500 Socket n Tx Read Pointer 1 Register (Socket Register Block)

			Sn_TX_WR0_REGISTER = 0x0024,	//	W5500 Socket n Tx Write Pointer 0 Register (Socket Register Block)
			Sn_TX_WR1_REGISTER = 0x0025,	//	W5500 Socket n Tx Write Pointer 1 Register (Socket Register Block)

			Sn_RX_RSR0_REGISTER = 0x0026,	//	W5500 Socket n Rx Received Size 0 Register (Socket Register Block)
			Sn_RX_RSR1_REGISTER = 0x0027,	//	W5500 Socket n Rx Received Size 1 Register (Socket Register Block)

			Sn_RX_RD0_REGISTER = 0x0028,	//	W5500 Socket n Rx Read Pointer 0 Register (Socket Register Block)
			Sn_RX_RD1_REGISTER = 0x0029,	//	W5500 Socket n Rx Read Pointer 1 Register (Socket Register Block)

			PHYCFGR_REGISTER = 0x002E,	//	W5500 PHY Configuration Register (Common Register Block)

			VERSIONR_REGISTER = 0x0039	//	W5500 Chip Version Register (Common Register Block)
		};

		enum configState_t{
			CONFIG_NONE,
			CONFIG_IP,
			CONFIG_GATEWAY,
			CONFIG_SUBNET,
			CONFIG_MAC,
			CONFIG_RX_BUFFER_SIZE,
			CONFIG_TX_BUFFER_SIZE,
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

		enum socketCloseMode_t{
			MANUAL_CLOSE,	//	User closes socket when wanted
			AUTO_CLOSE		//	Server closes socket when wanted
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

		enum socketInterruptStat_t : uint8_t{
			CON_INT = (1 << 0),
			DISCON_INT = (1 << 1),
			RECV_INT = (1 << 2),
			TIMEOUT_INT = (1 << 3),
			SEND_OK_INT = (1 << 4)
		};

		enum socketStat_t : uint8_t{
			SOCK_CLOSED = 0x00,
			SOCK_INIT = 0x13,
			SOCK_LISTEN = 0x14,
			SOCK_SYNSENT = 0x15,
			SOCK_ESTABLISHED = 0x17,
			SOCK_CLOSE_WAIT = 0x1C
		};


		enum socketBufferSize_t : uint8_t{
			SOCKBUF_1KB = 0x01,
			SOCKBUF_2KB = 0x02,
			SOCKBUF_4KB = 0x04,
			SOCKBUF_8KB = 0x08,
			SOCKBUF_16KB = 0x10
		};

		enum ethState_t{
			ETH_IDLE,
			//	CONFIG W550
			ETH_CONFIG_WRITE,
			ETH_CONFIG_WAIT_WRITE,
			ETH_CONFIG_NEXT,
			ETH_CONFIG_FINISHED,
			//	READ SOCKET STATUS
			ETH_SOCKET_STATUS_READ,
			ETH_SOCKET_STATUS_WAIT_READ,
			ETH_SOCKET_STATUS_CHECK,
			//	OPEN SOCKET
			ETH_SOCKET_OPEN_WRITE_MODE,
			ETH_SOCKET_OPEN_WAIT_WRITE_MODE,
			ETH_SOCKET_OPEN_WRITE_PORT,
			ETH_SOCKET_OPEN_WAIT_WRITE_PORT,
			ETH_SOCKET_OPEN_WRITE_COMMAND,
			ETH_SOCKET_OPEN_WAIT_WRITE_COMMAND,
			ETH_SOCKET_OPEN_CHECK_STATUS,
			ETH_SOCKET_OPEN_FINISHED,
			//	CONNECT SOCKET
			ETH_SOCKET_CONNECT_WRITE_IP,
			ETH_SOCKET_CONNECT_WAIT_WRITE_IP,
			ETH_SOCKET_CONNECT_WRITE_PORT,
			ETH_SOCKET_CONNECT_WAIT_WRITE_PORT,
			ETH_SOCKET_CONNECT_WRITE_COMMAND,
			ETH_SOCKET_CONNECT_WAIT_WRITE_COMMAND,
			ETH_SOCKET_CONNECT_CHECK_STATUS,
			ETH_SOCKET_CONNECT_FINISHED,
			//	SEND SOCKET
			ETH_SOCKET_SEND_READ_TX_FSR,
			ETH_SOCKET_SEND_WAIT_TX_FSR,
			ETH_SOCKET_SEND_READ_TX_WR,
			ETH_SOCKET_SEND_WAIT_READ_TX_WR,
			ETH_SOCKET_SEND_WRITE_BUFFER,
			ETH_SOCKET_SEND_WAIT_WRITE_BUFFER,
			ETH_SOCKET_SEND_WRITE_TX_WR,
			ETH_SOCKET_SEND_WAIT_WRITE_TX_WR,
			ETH_SOCKET_SEND_WRITE_COMMAND,
			ETH_SOCKET_SEND_WAIT_WRITE_COMMAND,
			ETH_SOCKET_SEND_READ_INTERRUPT_STAT,
			ETH_SOCKET_SEND_WAIT_READ_INTERRUPT_STAT,
			ETH_SOCKET_SEND_CLEAR_INTERRUPT,
			ETH_SOCKET_SEND_WAIT_CLEAR_INTERRUPT,
			ETH_SOCKET_SEND_FINISHED,
			//	RECEIVE SOCKET
			ETH_SOCKET_RCV_READ_RX_RSR,
			ETH_SOCKET_RCV_WAIT_READ_RX_RSR,
			ETH_SOCKET_RCV_READ_RX_RD,
			ETH_SOCKET_RCV_WAIT_READ_RX_RD,
			ETH_SOCKET_RCV_READ_BUFFER,
			ETH_SOCKET_RCV_WAIT_READ_BUFFER,
			ETH_SOCKET_RCV_WRITE_RX_RD,
			ETH_SOCKET_RCV_WAIT_WRITE_RX_RD,
			ETH_SOCKET_RCV_WRITE_COMMAND,
			ETH_SOCKET_RCV_WAIT_WRITE_COMMAND,
			ETH_SOCKET_RCV_CLEAR_INTERRUPT,
			ETH_SOCKET_RCV_WAIT_CLEAR_INTERRUPT,
			ETH_SOCKET_RCV_FINISHED,
			//	DISCONNECT SOCKET
			ETH_SOCKET_DISCONNECT_WRITE_COMMAND,
			ETH_SOCKET_DISCONNECT_WAIT_WRITE_COMMAND,
			ETH_SOCKET_DISCONNECT_CHECK_STATUS,
			ETH_SOCKET_DISCONNECT_FINISHED,
			//	CLOSE SOCKET
			ETH_SOCKET_CLOSE_WRITE_COMMAND,
			ETH_SOCKET_CLOSE_WAIT_WRITE_COMMAND,
			ETH_SOCKET_CLOSE_CHECK_STATUS,
			ETH_SOCKET_CLOSE_FINISHED
		};

		enum ethErrorState_t{
			ERROR_NONE,
			ERROR_TIMEOUT
		};

	private:
		uint8_t m_txBuffer[MAX_SPI_TRANSFER_LEN + 3];
		uint8_t m_rxBuffer[MAX_SPI_TRANSFER_LEN + 3];

		uint8_t *m_readBuffer;
		uint16_t m_readLen;

		volatile bool *m_doneFlag;
		bool m_pendingReadFlag;

		uint8_t m_ip[4];
		uint8_t m_gateway[4];
		uint8_t m_subnet[4];
		uint8_t m_mac[6];

		opMode_t m_opMode;
		configState_t m_currentConfigStat;
		bool m_initFinishedFlag;

		//	---------------	SOCKETS	---------------
		ethState_t m_ethState;
		ethErrorState_t m_ethError;

		SysTimer m_timeoutTimer;	//	To avoid blocking states
		bool m_timeoutFlag;

		uint16_t m_txBufferSize;
		uint16_t m_txBufferMask;
		uint16_t m_rxBufferSize;
		uint16_t m_rxBufferMask;
		bool m_pendingReadWrap;
		uint16_t m_firstReadLen;
		uint16_t m_secondReadLen;
		uint8_t *m_userReadBuffer;

		socketStat_t m_socketStat;
		uint8_t m_socketStatusByte;
		ethState_t m_nextStateAfterStatusRead;

		uint8_t m_localPortBuffer[2];
		uint8_t m_remotePortBuffer[2];
		uint8_t m_remoteIPBuffer[4];

		uint8_t *m_sendBuffer;
		uint16_t m_sendLen;
		uint8_t m_txFreeSize[2];			//	Read in Sn_TX_FSR
		uint8_t m_txWritePointer[2];		//	Read in Sn_TX_WR
		uint16_t m_nextTxWritePointer;		//	Updated with m_sendLen
		bool m_sendFinishedFlag;

		uint8_t *m_rcvBuffer;
		uint16_t m_actualRcvLen;			//	Size Received from Server
		uint16_t m_usrAskedRcvLen;			//	Size User wants to receive
		uint8_t m_rxReadPointer[2];			//	Read in Sn_RX_RD
		uint16_t m_nextRxReadPointer;		//	Updated with m_rcvLen
		uint8_t m_rxReceivedSize[2];		//	Read in Sn_RX_RSR
		bool m_rcvFinishedFlag;

		socketCloseMode_t m_socketCloseMode;

		uint8_t m_socketInterruptStat;

		volatile bool m_socketTransferDone;
		//	---------------------------------------------

		void transfer(uint16_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);		//	Makes a transfer (r/w) with W5500

		void readByte(uint16_t addr, block_t block, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Reads ONE BYTE from W5500 register
		uint8_t readByteAndWait(uint16_t addr, block_t block, opMode_t opMode = opMode_t::VAR_DATA_LEN);								//	Blocking version for debug
		void writeByte(uint16_t addr, block_t block, uint8_t data, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Writes ONE BYTE into W5500 register

		void readBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Reads [len] bytes from W5500 register and saves them in user [buffer]
		void readBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug
		void writeBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Writes [len] bytes into W5500 register from user [buffer]
		void writeBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug

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

		bool isBusy() const;		//	Returns True if Eth is busy

		void socketRequestStatus(ethState_t nextStateAfterStatusRead);	//	Sets next state after status read

		void timeoutError();	//	Handles ERROR_TIMEOUT

	public:

		Eth(bool portCS, uint8_t pinCS, Spi &spi);	//	Constructor

		void init(uint8_t ip[4], uint8_t gateway[4], uint8_t subnet[4], uint8_t mac[6], socketBufferSize_t rxBufferSize = socketBufferSize_t::SOCKBUF_2KB, socketBufferSize_t txBufferSize = socketBufferSize_t::SOCKBUF_2KB, socketCloseMode_t closeMode = socketCloseMode_t::AUTO_CLOSE ,opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Initializes W5500 Module

		bool isLinkUp();						//	Returns True if Link is Up (Electrical Connection between W5500 and Router)
		socketStat_t socketStatus() const;		//	Returns Socket Status
		bool isReady() const;					//	Return true if eth can make an operation

		void socketOpenTCP(uint16_t localPort);							//	Opens Socket, TCP Socket Mode
		bool socketOpened() const;										//	Returns True if Socket is Opened
		void socketConnect(uint8_t remoteIP[4], uint16_t remotePort);	//	Connects Socket
		bool socketConnected() const;									//	Returns True if Socket is Connected
		void socketSend(uint8_t *buffer, uint16_t len);					//	Sends Data to Server
		bool socketSendFinished() const;								//	Returns True if Data was sent to Server
		void socketReceive(uint8_t *buffer, uint16_t maxLen);			//	Receives Data from Server
		uint16_t socketReceivedLen() const;								//	Returns Size of Data Received from Server
		bool socketReceiveFinished() const;								//	Returns True if Data was received from Server
		void socketDisconnect();										//	Disconnects Socket
		bool socketDisconnectFinished() const;							//	Returns True if Socket was Disconnected
		void socketClose();												//	Closes Socket
		bool socketCloseFinished() const;								//	Returns True if Socket was Closed

		void handler();			//	Non-blocking W5500 handler

		~Eth();		//	Destructor
};

#endif /* ETH_H_ */
