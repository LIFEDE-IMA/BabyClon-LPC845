/*
 * eth.h
 *
 *  Created on: 17 jul. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This class was written to handle W5500 Ethernet module with SPI peripheral
 *
 * 	NOTES:	Sometimes, when connecting socket TCP, SPI reads 0x19 in Sn_SR register (socket status).
 * 			0x19 is NOT a valid option according to Wiznet datasheet, but if you read again after getting
 * 			0x19 then you get a valid state. Not sure why this happens but added 0x19 as "transient" socket status.
 *
 * 			Using DNS provided by DHCP does not always resolve domain's IP.
 * 			Cause of this, 8.8.8.8 Google's DNS is hardcoded to resolve any domain.
 * 			(1.1.1.1 worked too when tests run)
 */

#ifndef ETH_H_
#define ETH_H_

#include "spi.h"
#include "systimer.h"
#include "my_string.h"

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

		enum initConfigMode_t : uint8_t{
			INIT_WITH_STATIC_IP,
			INIT_WITH_DHCP
		};

		enum configState_t : uint8_t{
			CONFIG_NONE,
			CONFIG_MAC,
			CONFIG_RX_BUFFER_SIZE,
			CONFIG_TX_BUFFER_SIZE,
			CONFIG_IP,
			CONFIG_GATEWAY,
			CONFIG_SUBNET
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
			SOCK_EXCEPTION_TRANSIENT_STAT = 0x19,	//	Not valid according Wiznet datasheet, but read several times in Sn_SR
			SOCK_CLOSE_WAIT = 0x1C,
			SOCK_UDP = 0x22
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
			//	DHCP
			ETH_DHCP_START,
			ETH_DHCP_BUILD_DISCOVER,
			ETH_DHCP_WAIT_OFFER,
			ETH_DHCP_PARSE_OFFER,
			ETH_DHCP_VALIDATE_OPTIONS,
			ETH_DHCP_VALIDATE_OFFER,
			ETH_DHCP_BUILD_REQUEST,
			ETH_DHCP_WAIT_ACK,
			ETH_DHCP_PARSE_ACK,
			ETH_DHCP_VALIDATE_ACK,
			ETH_DHCP_FINISHED,
			//	DNS
			ETH_DNS_BUILD_QUERY,
			ETH_DNS_SEND_QUERY,
			ETH_DNS_WAIT_RESPONSE,
			ETH_DNS_PARSE_RESPONSE,
			ETH_DNS_FINISHED,
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
			//	CONNECT SOCKET ( TCP ) / SET DESTINATION ( UDP )
			ETH_SOCKET_WRITE_IP,
			ETH_SOCKET_WAIT_WRITE_IP,
			ETH_SOCKET_WRITE_PORT,
			ETH_SOCKET_WAIT_WRITE_PORT,
			//	CONNECT SOCKET ( TCP )
			ETH_SOCKET_CONNECT_WRITE_COMMAND,
			ETH_SOCKET_CONNECT_WAIT_WRITE_COMMAND,
			ETH_SOCKET_CONNECT_CHECK_STATUS,
			ETH_SOCKET_CONNECT_FINISHED,
			//	SEND SOCKET ( TCP & UDP )
			ETH_SOCKET_SEND_READ_TX_FSR,
			//	SEND SOCKET ( TCP )
			ETH_SOCKET_SEND_TCP_WAIT_TX_FSR,
			//	SEND SOCKET ( UDP )
			ETH_SOCKET_SEND_UDP_WAIT_TX_FSR,
			//	SEND SOCKET ( TCP & UDP )
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
			//	RECEIVE SOCKET ( TCP & UDP )
			ETH_SOCKET_RCV_READ_RX_RSR,
			ETH_SOCKET_RCV_WAIT_READ_RX_RSR,
			ETH_SOCKET_RCV_READ_RX_RD,
			ETH_SOCKET_RCV_WAIT_READ_RX_RD,
			//	RECEIVE SOCKET ( TCP )
			ETH_SOCKET_RCV_TCP_READ_BUFFER,
			ETH_SOCKET_RCV_TCP_WAIT_READ_BUFFER,
			//	RECEIVE SOCKET ( UDP )
			ETH_SOCKET_RCV_UDP_READ_HEADER,
			ETH_SOCKET_RCV_UDP_WAIT_READ_HEADER,
			ETH_SOCKET_RCV_UDP_READ_PAYLOAD,
			ETH_SOCKET_RCV_UDP_WAIT_READ_PAYLOAD,
			//	RECEIVE SOCKET ( TCP & UDP )
			ETH_SOCKET_RCV_WRITE_RX_RD,
			ETH_SOCKET_RCV_WAIT_WRITE_RX_RD,
			ETH_SOCKET_RCV_WRITE_COMMAND,
			ETH_SOCKET_RCV_WAIT_WRITE_COMMAND,
			ETH_SOCKET_RCV_CLEAR_INTERRUPT,
			ETH_SOCKET_RCV_WAIT_CLEAR_INTERRUPT,
			ETH_SOCKET_RCV_FINISHED,
			//	DISCONNECT SOCKET ( TCP )
			ETH_SOCKET_DISCONNECT_WRITE_COMMAND,
			ETH_SOCKET_DISCONNECT_WAIT_WRITE_COMMAND,
			ETH_SOCKET_DISCONNECT_CHECK_STATUS,
			ETH_SOCKET_DISCONNECT_FINISHED,
			//	CLOSE SOCKET ( TCP & UDP )
			ETH_SOCKET_CLOSE_CLEAR_INTERRUPT,
			ETH_SOCKET_CLOSE_WAIT_CLEAR_INTERRUPT,
			ETH_SOCKET_CLOSE_WRITE_COMMAND,
			ETH_SOCKET_CLOSE_WAIT_WRITE_COMMAND,
			ETH_SOCKET_CLOSE_CHECK_STATUS,
			ETH_SOCKET_CLOSE_FINISHED
		};

		enum ethErrorStat_t{
			ERROR_NONE,
			ERROR_TIMEOUT,
			ERROR_SOCK_CLOSED,
			ERROR_DNS_INVALID_DOMAIN,
			ERROR_DNS_INVALID_RESPONSE
		};

		static const uint8_t DNS_MAX_DOMAIN_LEN = 253;

	private:
		enum transferContext_t : uint8_t{
			NONE,
			GENERIC,
			READ_BUFFER,
			WRITE_BUFFER
		};

		uint8_t m_txBuffer[MAX_SPI_TRANSFER_LEN + 3];
		uint8_t m_rxBuffer[MAX_SPI_TRANSFER_LEN + 3];

		//	---------------	TRANSFER ---------------

		transferContext_t m_transferContext;

		uint16_t m_transferAddr;
		uint16_t m_transferRemainingLen;
		uint16_t m_transferProcessedLen;

		uint8_t *m_transferData;
		uint8_t m_transferByte;

		block_t m_transferBlock;
		rwMode_t m_transferRWMode;
		opMode_t m_transferOpMode;

		volatile bool m_transferBlockDoneFlag;
		bool m_transferInProgressFlag;

		volatile bool *m_usrDoneFlag;

		//	---------------	READ / WRITE BUFFER	---------------

		uint8_t *m_usrBufferData;
		uint16_t m_usrBufferRemainingLen;

		bool m_w5500WrapAroundFlag;

		uint16_t m_w5500BufferSize;
		uint16_t m_w5500BufferMask;
		uint16_t m_w5500BufferCurrentAddr;
		uint16_t m_w5500BufferCurrentLen;

		//	---------------	INIT CONFIG	---------------

		uint8_t m_ip[4];
		uint8_t m_gateway[4];
		uint8_t m_subnet[4];
		uint8_t m_mac[6];

		opMode_t m_opMode;
		initConfigMode_t m_initConfigMode;
		configState_t m_currentConfigStat;
		bool m_initFinishedFlag;

		//	---------------	SOCKETS	---------------
		ethState_t m_ethState;
		ethErrorStat_t m_ethError;
		ethState_t m_ethStateWhenLastError;
		socketMode_t m_socketMode;

		SysTimer m_timeoutTimer;	//	To avoid blocking states
		bool m_timeoutFlag;

		uint16_t m_txBufferSize;
		uint16_t m_txBufferMask;
		uint16_t m_rxBufferSize;
		uint16_t m_rxBufferMask;

		socketStat_t m_socketStat;
		uint8_t m_socketStatusByte;
		ethState_t m_nextStateAfterStatusRead;

		uint8_t m_localPortBuffer[2];
		uint8_t m_remotePortBuffer[2];
		uint8_t m_remoteIPBuffer[4];

		uint8_t *m_sendBuffer;
		uint16_t m_sendLen;
		uint16_t m_sendProcessedLen;
		uint16_t m_sendRemainingLen;
		uint16_t m_sendCurrentLen;
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

		//	---------------	UDP	---------------
		bool m_destinationSetFlag;

		uint8_t m_headerUDP[8];
		static const uint8_t UDP_HEADER_LEN = 8;

		uint8_t m_udpRcvRemoteIP[4];
		uint8_t m_udpRcvRemotePort[2];
		uint16_t m_udpPayloadLen;

		//	---------------		DCHP	---------------

		static const uint16_t DHCP_BUFFER_LEN = 512;
		static const uint8_t DHCP_FIXED_HEADER_LEN = 236;
		static const uint8_t DHCP_MAGIC_COOKIE_LEN = 4;

		static const uint8_t DHCP_CLIENT_PORT = 68;
		static const uint8_t DHCP_SERVER_PORT = 67;

		static const uint8_t DHCP_OP_BOOT_REQUEST = 1;
		static const uint8_t DHCP_OP_BOOT_REPLY = 2;

		static const uint8_t DHCP_HTYPE_ETHERNET = 1;
		static const uint8_t DHCP_HLEN_ETHERNET = 6;

		static const uint8_t DHCP_DISCOVER = 1;
		static const uint8_t DHCP_OFFER	 = 2;
		static const uint8_t DHCP_REQUEST = 3;
		static const uint8_t DHCP_DECLINE = 4;
		static const uint8_t DHCP_ACK = 5;
		static const uint8_t DHCP_NACK = 6;

		static const uint8_t DHCP_OPTION_PAD = 0;
		static const uint8_t DHCP_OPTION_SUBNET_MASK = 1;
		static const uint8_t DHCP_OPTION_ROUTER = 3;
		static const uint8_t DHCP_OPTION_DNS = 6;
		static const uint8_t DHCP_OPTION_REQUESTED_IP = 50;
		static const uint8_t DHCP_OPTION_LEASE_TIME = 51;
		static const uint8_t DHCP_OPTION_MESSAGE_TYPE = 53;
		static const uint8_t DHCP_OPTION_SERVER_IDENTIFIER = 54;
		static const uint8_t DHCP_OPTION_PARAMETER_REQUEST = 55;
		static const uint8_t DHCP_OPTION_CLIENT_IDENTIFIER = 61;
		static const uint8_t DHCP_OPTION_END = 255;

		enum dhcpState_t : uint8_t{
			DHCP_IDLE,
			DHCP_START,
			DHCP_BUILD_DISCOVER,
			DHCP_WAIT_OFFER,
			DHCP_PARSE_OFFER,
			DHCP_BUILD_REQUEST,
			DHCP_WAIT_ACK,
			DHCP_PARSE_ACK,
			DHCP_FINISHED
		};

		dhcpState_t m_dhcpState;

		uint8_t m_dhcpRxBuffer[DHCP_BUFFER_LEN];
		uint8_t m_dhcpTxBuffer[DHCP_BUFFER_LEN];

		uint16_t m_dhcpRxLen;
		uint16_t m_dhcpTxLen;

		uint8_t m_dhcpRetryCount;

		uint8_t m_dhcpTransactionID[4];
		uint8_t m_dhcpOfferedIP[4];
		uint8_t m_dhcpServerIP[4];
		uint8_t m_dhcpDNS[4];

		uint16_t m_dhcpCurrentRxIndex;

		uint32_t m_dhcpLeaseTime;

		bool m_dhcpMsgTypeFound_flag;
		bool m_dhcpServerIpFound_flag;
		bool m_dhcpSubnetFound_flag;
		bool m_dhcpGatewayFound_flag;
		bool m_dhcpDnsFound_flag;
		bool m_dhcpLeaseTimeFound_flag;

		bool m_dhcpInProgressFlag;
		bool m_dhcpOptionErrorFlag;
		bool m_dhcpOptionNACKfound;
		bool m_dhcpFinishedFlag;

		//	---------------		DNS		---------------

		enum dnsParseState_t : uint8_t{
			DNS_PARSE_NONE,
			DNS_PARSE_HEADER,
			DNS_PARSE_QUESTION_NAME,
			DNS_PARSE_QUESTION_TYPE,
			DNS_PARSE_ANSWER_NAME,
			DNS_PARSE_ANSWER_FIXED,
			DNS_PARSE_ANSWER_DATA,
			DNS_PARSE_SUCCESS,
			DNS_PARSE_ERROR
		};

		dnsParseState_t m_dnsParseState;

		static const uint16_t DNS_PORT = 53;
		static const uint16_t DNS_BUFFER_LEN = 512;

		uint8_t m_dnsQueryBuffer[Eth::DNS_BUFFER_LEN];
		uint8_t m_dnsRxBuffer[Eth::DNS_BUFFER_LEN];

		uint16_t m_dnsQueryLen;
		uint16_t m_dnsRxLen;

		uint16_t m_dnsTransactionID;

		char m_dnsDomain[(Eth::DNS_MAX_DOMAIN_LEN + 1)];
		uint8_t m_dnsServerIP[4];
		uint8_t m_dnsResolvedIP[4];

		uint16_t m_dnsCurrentIndex;

		uint16_t m_dnsQDcount;
		uint16_t m_dnsANcount;
		uint16_t m_dnsAnswRemaining;
		uint16_t m_dnsAnswType;
		uint16_t m_dnsAnswClass;
		uint16_t m_dnsAnswDataLen;

		bool m_dnsFinishedFlag;
		bool m_dnsInProgressFlag;

		//	---------------		HTTP		---------------

		enum httpState_t : uint8_t{
			HTTP_IDLE,
			HTTP_CONNECT,
			HTTP_SEND,
			HTTP_RCV,
			HTTP_CHECK,
			HTTP_SUCCESS,
			HTTP_FINISHED,
			HTTP_ERROR
		};

		enum httpError_t : uint8_t{
			HTTP_ERROR_NONE,
			HTTP_ERROR_BUILDING,
			HTTP_ERROR_OPEN_TIMEOUT,
			HTTP_ERROR_CONNECT_TIMEOUT,
			HTTP_ERROR_CONN_SOCK_CLOSED,
			HTTP_ERROR_SEND_TIMEOUT,
			HTTP_ERROR_RCV_TIMEOUT,
			HTTP_ERROR_CHECK_RESPONSE,
			HTTP_ERROR_DISCONNECT_TIMEOUT
		};

		httpState_t m_httpState;
		httpError_t m_httpError;

		static const uint16_t HTTP_MAX_RQST_LEN = 400;
		static const uint8_t HTTP_MAX_BDY_LEN = 150;
		static const uint8_t HTTP_MAX_SERVER_PATH_LEN = 250;
		static const uint8_t HTTP_MAX_USR_AGENT_LEN = 50;
		static const uint16_t HTTP_MAX_RESPONSE_LEN = 512;

		char m_httpServerHost[(Eth::DNS_MAX_DOMAIN_LEN + 1)];
		char m_httpServerPath[Eth::HTTP_MAX_SERVER_PATH_LEN];

		char m_httpRequest[Eth::HTTP_MAX_RQST_LEN];
		uint16_t m_httpRequestLen;

		char m_httpBody[Eth::HTTP_MAX_BDY_LEN];
		uint8_t m_httpBodyLen;

		char m_httpUsrAgent[Eth::HTTP_MAX_USR_AGENT_LEN];

		char m_httpServerDataPath[Eth::HTTP_MAX_SERVER_PATH_LEN];

		char m_httpServerResponse[Eth::HTTP_MAX_RESPONSE_LEN];
		uint16_t m_httpServerResponseLen;

		uint16_t m_httpServerPort;

		bool m_httpInProgressFlag;
		bool m_httpFinishedFlag;
		bool m_httpHeartbeatInProgressFlag;
		bool m_httpHeartbeatFinishedFlag;
		bool m_httpErrorOccurred;

		bool m_httpUploading_usrFlag;		//	USR API
		bool m_httpHeartBeating_usrFlag;	//	USR API

		//	-----------------------------------

		void transferBlock(uint16_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Makes ONE transfer of [ MAX_SPI_TRANSFER_LEN ] bytes
		void transfer(uint16_t addr, block_t block, rwMode_t rwMode, uint8_t *data, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);			//	Makes a transfer of X needed BLOCKS ( X * MAX_SPI_TRANSFER_LEN bytes) (r/w) with W5500
		void startNextBufferTransfer();	//	Sets next transfer() depending on whether or not theres a wrap-around

		void readByte(uint16_t addr, block_t block, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Reads ONE BYTE from W5500 register
		uint8_t readByteAndWait(uint16_t addr, block_t block, opMode_t opMode = opMode_t::VAR_DATA_LEN);								//	Blocking version for debug
		void writeByte(uint16_t addr, block_t block, uint8_t data, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Writes ONE BYTE into W5500 register

		void readBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Reads [len] bytes from W5500 register and saves them in user [buffer]
		bool readBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug
		void writeBuffer(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Writes [len] bytes into W5500 register from user [buffer]
		void writeBufferAndWait(uint16_t addr, block_t block, uint8_t *buffer, uint16_t len, opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug

		void setIP(uint8_t ip[4], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);				//	Sets SIPRn Registers in W5500
		void setIPAndWait(uint8_t ip[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);								//	Blocking version for debug
		bool readIP(uint8_t ip[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);									//	Reads SIPRn Registers in W5500

		void setGateway(uint8_t gateway[4], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Sets GARn Registers in W5500
		void setGatewayAndWait(uint8_t gateway[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);					//	Blocking version for debug
		bool readGateway(uint8_t gateway[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);							//	Reads GARn Registers in W5500

		void setSubnet(uint8_t subnet[4], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);		//	Sets SUBRn Registers in W5500
		void setSubnetAndWait(uint8_t subnet[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);						//	Blocking version for debug
		bool readSubnet(uint8_t subnet[4], opMode_t opMode = opMode_t::VAR_DATA_LEN);							//	Reads SUBRn Registers in W5500

		void setMAC(uint8_t mac[6], volatile bool *f_done, opMode_t opMode = opMode_t::VAR_DATA_LEN);			//	Sets SHARn Registers in W5500
		void setMACAndWait(uint8_t mac[6], opMode_t opMode = opMode_t::VAR_DATA_LEN);							//	Blocking version for debug
		bool readMAC(uint8_t mac[6], opMode_t opMode = opMode_t::VAR_DATA_LEN);									//	Reads SHARn Registers in W5500

		bool isBusy() const;		//	Returns True if Eth is busy

		void socketRequestStatus(ethState_t nextStateAfterStatusRead);	//	Sets next state after status read

		void socketOpen(socketMode_t sockMode, uint16_t localPort);		//	Opens Socket, TCP / UDP Socket Mode
		bool socketOpened() const;										//	Returns True if Socket is Opened
		void socketTCPconnect(uint8_t remoteIP[4], uint16_t remotePort);//	Connects Socket, TCP Socket Mode
		void socketTCPconnect(uint16_t remotePort);						//	DNS version of TCPconnect
		bool socketTCPconnected() const;								//	Returns True if TCP Socket is Connected
		void socketUDPsetDest(uint8_t remoteIP[4], uint16_t remotePort);//	Sets destination IP and port in UDP Socket Mode
		void socketUDPsetDest(uint16_t remotePort);						//	DNS version of UDPsetDest
		bool socketUDPdestSet() const;									//	Returns True if Socket Destination IP and port was set in UDP Socket Mode
		void socketTCPsend(const void *buffer, uint16_t len);			//	Sends Data to Server, TCP Socket Mode
		void socketUDPsend(uint8_t *buffer, uint16_t len);				//	Sends Data to Server, UDP Socket Mode
		bool socketSendFinished() const;								//	Returns True if Data was sent to Server
		void socketTCPreceive(const void *buffer, uint16_t maxLen);		//	Receives Data from Server, TCP Socket Mode
		void socketUDPreceive(uint8_t *buffer, uint16_t maxLen);		//	Receives Data from Server, UDP Socket Mode
		uint16_t socketReceivedLen() const;								//	Returns Size of Data Received from Server
		bool socketReceiveFinished() const;								//	Returns True if Data was received from Server
		void socketTCPdisconnect();										//	Disconnects Socket, TCP Socket Mode
		bool socketTCPdisconnectFinished() const;						//	Returns True if TCP Socket was Disconnected
		void socketClose();												//	Closes Socket
		bool socketCloseFinished() const;								//	Returns True if Socket was Closed

		void DHCPgenerateXid();		//	Generates Transaction ID for DHCP
		void DHCPstart();			//	Starts Dynamic Host Configuration Protocol (gets local IP, Subnet & Gateway)
		void DHCPbuildDiscover();	//	Builds DHCPDISCOVER
		void DHCPwaitOffer();		//	Starts UDP Socket Receive for DHCP offer
		void DHCPvalidateOption(uint8_t expectedMsgType, ethState_t nextStateIfOptionOK);	//	Evaluates if DHCP Offer Option is OK
		bool DHCPparseOffer();		//	Returns True if DHCP Offer is OK to build DHCPREQUEST
		void DHCPbuildRequest();	//	Builds DHCPREQUEST
		void DHCPwaitACK();			//	Prepares the driver before reading DHCP ACK/NACK
		bool DHCPparseACK();		//	Returns True if DHCP ACK is OK so we can finish DHCP init
		bool DHCPfinished() const;	//	Returns True when acquired IP, Subnet & Gateway

		void DNSgenerateXid();			//	Generates Transaction ID for DNS
		void DNSbuildQuery();			//	Builds	QUERY DNS
		void DNSsetRandomLocalPort();	//	Modifies m_localPortBuffer to be "random"
		void DNSsetRemoteIPandPort();	//	Modifies m_remoteIPBuffer & m_remotePortBuffer
		void DNSwaitResponse();			//	Prepares the driver before reading DNS RESPONSE
		void DNSparseResponse();		//	Handles DNS RESPONSE and checks if its OK to end DNS operation

		uint8_t HTTPbuildBody(const char *data);			//	Builds body for our POST HTTP
		uint16_t HTTPbuildRequest();						//	Builds request for out POST HTTP
		void HTTPcheckResponse();							//	Checks if server response is OK
		void HTTPuploadData();								//	Uploads data to server (POST HTTP)
		void HTTPheartbeat();								//	Uploads heartbeat to server (POST HTTP)
		void HTTPtimeoutError(ethState_t currentEthState);	//	Configures m_httpError when timeout occurs

		void timeoutError();			//	Handles ERROR_TIMEOUT

		void SPItransferHandler();			//	Handles SPI operations (read/write buffer)
		void W5500configStateMachine();		//	Handles MAC, IP, SUBNET, GATEWAY & BUFFERS_SIZE W5500 configuration
		void W5500selectNextConfigState();	//	Selects which state W5500configStateMachine() should config

	public:
		Eth(bool portCS, uint8_t pinCS, Spi &spi);	//	Constructor

		void init(uint8_t ip[4], uint8_t gateway[4], uint8_t subnet[4], uint8_t mac[6],
				  socketBufferSize_t rxBufferSize = socketBufferSize_t::SOCKBUF_2KB,
				  socketBufferSize_t txBufferSize = socketBufferSize_t::SOCKBUF_2KB,
				  socketCloseMode_t closeMode = socketCloseMode_t::MANUAL_CLOSE,
				  opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Initializes W5500 Module with Static IP

		void init(uint8_t mac[6], socketBufferSize_t rxBufferSize = socketBufferSize_t::SOCKBUF_2KB,
				  socketBufferSize_t txBufferSize = socketBufferSize_t::SOCKBUF_2KB,
				  socketCloseMode_t closeMode = socketCloseMode_t::MANUAL_CLOSE,
				  opMode_t opMode = opMode_t::VAR_DATA_LEN);	//	Initializes W5500 Module with DHCP

		bool isLinkUp();						//	Returns True if Link is Up (Electrical Connection between W5500 and Router)
		socketStat_t socketStatus() const;		//	Returns Socket Status
		bool isReady() const;					//	Return true if eth can make an operation
		ethErrorStat_t currentError() const;	//	Returns Socket Current Error Stat
		ethState_t stateWhenLastError() const;	//	Returns Eth State in which ocurred last error

		void DNSresolve(const char *domain);	//	Starts DNS operation to acquire server ip
		bool DNSresolveFinished() const;		//	Returns True if DNS operation ended

		void HTTPuploadData(uint16_t localPort, uint16_t serverPort, const char *serverPath, const char *serverDataPath, const char *device, const char *data);	//	Starts data upload to server
		bool HTTPdataUploaded() const;		//	Returns true if data was uploaded
		void HTTPheartbeat(uint16_t localPort, uint16_t serverPort, const char *serverPath, const char *device);	//	Sends life proof to server
		bool HTTPheartbeatFinished() const;	//	Returns true if heartbeat was uploaded
		void HTTPuploading(bool flag);		//	Usr sets uploading as true/false
		bool HTTPuploading() const;			//	Returns m_httpUploading_usrFlag
		void HTTPheartBeating(bool flag);	//	Usr sets heartBeating as true/false
		bool HTTPheartBeating() const;		//	Returns m_httpHeartBeating_usrFlag
		bool HTTPisBusy() const;			//	USR API
		bool HTTPerrorOccurred() const;		//	Returns true if http had an Error
		void HTTPrestartAfterError();		//	Prepares driver for its restart after an error occurred

		void stateMachine();				//	Non-blocking W5500 handler

		~Eth();		//	Destructor
};

#endif /* ETH_H_ */
