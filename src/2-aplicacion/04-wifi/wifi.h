/*
 * wifi.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle ESP8266 with the LPC845, being ESP8266 the wifi module
 *  The protocol chosen to communicate with the server was POST PROTOCOL
 */

#ifndef WIFI_H_
#define WIFI_H_

#include "my_string.h"
#include "uart.h"
#include "systimer.h"

class WiFi{
	public:
		enum wifiMode_t : uint8_t{
			WIFI_STATION_MODE = 1,
			WIFI_ACCESS_POINT_MODE = 2,
			WIFI_STATION_AND_ACCESS_POINT_MODE = 3
		};

	private:
		static const uint16_t MAX_AT_CMD_LEN = 300;			//	Max command AT length
		static const uint8_t MAX_SRCH_LEN = 128;			//	Max search buffer length
		static const uint8_t HTTP_MAX_BDY_LEN = 150;		//	Max body length (POST PROTOCOL)
		static const uint16_t HTTP_MAX_RQST_LEN = 400;		//	Max request length (POST PROTOCOL)
		static const uint8_t HTTP_MAX_USR_AGENT_LEN = 50;	//	Max user agent (device name) length (POST PTROCOL)
		static const uint8_t HTTP_MAX_SERVER_PATH_LEN = 250;//	(POST PROTOCOL)
		static const uint8_t DNS_MAX_DOMAIN_LEN = 253;		//	Max domain length

		static const uint8_t SRCH_SUCCEED = 1;
		static const uint8_t SRCH_FAILED = 2;

		Uart &m_wifiUart;		//	Uart thats gonna manage wifi comm

		wifiMode_t m_wifiMode;

		bool m_wifiInitializedFlag;
		bool m_wifiUploadingDataFlag;
		bool m_wifiUploadFinishedFlag;
		bool m_wifiHeartbeatingFlag;
		bool m_wifiHeartbeatFinishedFlag;

		char m_httpBody[WiFi::HTTP_MAX_BDY_LEN];
		char m_httpRequest[WiFi::HTTP_MAX_RQST_LEN];
		uint8_t m_httpBodyLen;
		uint16_t m_httpRequestLen;
		char m_httpUsrAgent[WiFi::HTTP_MAX_USR_AGENT_LEN];
		char m_httpServerDataPath[WiFi::HTTP_MAX_SERVER_PATH_LEN];
		char m_httpServerPath[WiFi::HTTP_MAX_SERVER_PATH_LEN];
		char m_httpServerHost[(WiFi::DNS_MAX_DOMAIN_LEN + 1)];
		uint16_t m_httpServerPort;

		char m_searchBuffer[WiFi::MAX_SRCH_LEN];
		uint16_t m_searchIndex;
		char m_ATcmdBuffer[WiFi::MAX_AT_CMD_LEN];

		//	DEBUG:
		static const uint8_t RX_LOG_LEN = 128;
		char m_rxLog[WiFi::RX_LOG_LEN];
		uint8_t m_rxLogIndex;

		enum initStates_t{
			iSTARTUP_WAIT,
			iIDLE,
			iWAIT_ATE0,
			iWAIT_ATE0_OK,
			iSET_MODE,
			iWAIT_SETMODE,
			iWAIT_MODE_OK,
			iSEND_CREDENTIALS,
			iWAIT_SENDCRED,
			iWAIT_DISCONNECT,
			iWAIT_CONNECTED,
			iWAIT_GOTIP,
			iWAIT_IPOK,
			iDONE,
			iERROR
		};

		enum uploadStates_t{
			uIDLE,
			uWAIT_SENDOPEN,
			uWAIT_CONNECT,
			uSEND_LEN,
			uWAIT_SENDLEN,
			uWAIT_LENOK,
			uWAIT_PROMPT,
			uSEND_REQUEST,
			uWAIT_SENDOK,
			uWAIT_200OK,
			uSEND_CLOSE,
			uWAIT_SENDCLOSE,
			uWAIT_CLOSED,
			uDONE,
			uERROR
		};

		enum errors_t{
			NONE,
			IE_CWMODE,		//	Init Error
			IE_CWJAP,
			UE_CIPSTART,	//	Upload Error
			UE_CIPSEND,
			UE_SENDOK,
			UE_200OK,
			UE_TIMEOUT
		};

		initStates_t m_initState;
		uploadStates_t m_uploadState;
		errors_t m_wifiError;

		static const uint8_t STARTUP_DELAY_S = 2;
		static const uint8_t CMD_TIMEOUT_S = 5;
		static const uint8_t CWJAP_TIMEOUT_S = 20;
		static const uint8_t UPLOAD_TIMEOUT_S = 10;

		SysTimer m_timeout;

		uint8_t search4(const char* target);			//	Searches an expected answer from ESP into rx buffer

		void buildAT(String &AT, wifiMode_t wifiMode, initStates_t initState);	//	Builds AT cmd string according to wifiMode selected

		uint8_t buildBody(const char *data);	//	Builds body for HTTP POST protocol
		uint16_t buildRequest(void);			//	Builds the request for the server (HTTP POST protocol)

		bool openConnection();		//	Opens connection to server
		bool buildRequestLen();		//	Sends request length to server
		bool sendRequest();			//	Sends request to server

		void handler( const char *serverDomain, uint16_t serverPort,  const char *serverPath, const char *device, const char *serverDataPath = nullptr, const char *data = nullptr);			//	Handles Heartbeating / Uploading

	public:
		WiFi(Uart &uart);		//	Constructor

		void init(const char* ssid, const char* pass, wifiMode_t wifiMode = wifiMode_t::WIFI_STATION_MODE);	//	States machine that connects ESP8266 to a wifi network
		bool initFinished(void) const;	//	Returns true if wifi was intialized

		void heartbeat(const char *serverDomain, uint16_t serverPort, const char *serverPath, const char *device);	//	Uploads Heartbeat to Server
		bool heartbeatFinished(void) const;	//	Returns True if Heartbeat was Uploaded

		void uploadData(const char *serverDomain, uint16_t serverPort,  const char *serverPath, const char *serverDataPath, const char *device, const char *data);	//	States machine that uploads data (strings) to server
		bool uploadFinished(void) const;	//	Returns true if data was uploaded

		bool closeConnection();		//	Closes connection with server

		~WiFi();	//	Destructor
};


#endif /* WIFI_H_ */
