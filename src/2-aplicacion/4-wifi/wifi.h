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

#include "uart.h"
#include "my_string.h"

#define MAX_CMD_LEN 300		//	Max command AT length
#define MAX_BDY_LEN 150		//	Max body length (POST PROTOCOL)
#define MAX_RQST_LEN 400	//	Max request length (POST PROTOCOL)

#define YES	1	//	Macro for states machine
#define NO	2	//	Macro for states machine

class WiFi{
	private:
		Uart &m_wUart;		//	Uart thats gonna manage wifi comm

		char m_httpBody[MAX_BDY_LEN];
		char m_httpRequest[MAX_RQST_LEN];
		uint8_t m_httpBodyLen;
		uint16_t m_httpRequestLen;

		char m_cmdBuffer[MAX_CMD_LEN];

		enum initStates_t{
			iIDLE,
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
			uWAIT_SENDRQST,
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
			UE_200OK
		};

		initStates_t m_initState;
		uploadStates_t m_uploadState;
		errors_t m_wifiError;

	public:
		WiFi(Uart &uart);		//	Constructor

		uint8_t search4(const char* target);			//	Searches an expected answer from ESP into rx buffer

		bool init(const char* ssid, const char* pass);	//	States machine that connects ESP8266 to a wifi network

		uint8_t makeBody(const char *device, const char *path, char *data);	//	Builds body for HTTP POST protocol
		uint16_t makeRequest(const char *device);							//	Builds the request for the server

		void openConnection();		//	Opens connection to server
		bool buildRequestLen();		//	Sends request length to server
		void sendRequest();			//	Sends request to server
		void closeConnection();		//	Closes connection with server

		bool uploadData(const char *device, const char *path, char *data);	//	States machine that uploads data (strings) to server

		~WiFi();	//	Destructor
};


#endif /* WIFI_H_ */
