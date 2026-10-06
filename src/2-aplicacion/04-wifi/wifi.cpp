/*
 * wifi.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "wifi.h"

WiFi::WiFi(Uart &uart) : m_wifiUart(uart){
	m_initState = initStates_t::iIDLE;
	m_uploadState = uploadStates_t::uIDLE;
	m_wifiError = errors_t::NONE;
	m_searchIndex = 0;
	m_wifiInitializedFlag = false;
	m_wifiUploadFinishedFlag = false;
	for(uint8_t index = 0; index < WiFi::MAX_SRCH_LEN; index++)	m_searchBuffer[index] = '\0';
	for(uint16_t index = 0; index < WiFi::MAX_AT_CMD_LEN; index++) m_ATcmdBuffer[index] = '\0';
}

uint8_t WiFi::search4(const char* target){
	int16_t recvByte = m_wifiUart.receiveByte();

	if(recvByte != Uart::NO_DATA_RECEIVED){
		if(m_searchIndex < (WiFi::MAX_SRCH_LEN - 1)){
			m_searchBuffer[m_searchIndex++] = recvByte;
			m_searchBuffer[m_searchIndex] = '\0';
		}else{	//	If searchBuffer fills up, keeps the most recent half to avoid the risk of losing the target
			for(uint8_t idx = 0; idx < (WiFi::MAX_SRCH_LEN / 2); idx++)
				m_searchBuffer[idx] = m_searchBuffer[(idx + (MAX_SRCH_LEN / 2))];

			m_searchIndex = (MAX_SRCH_LEN / 2);
			m_searchBuffer[m_searchIndex] = '\0';
		}

		if(String::strstr(m_searchBuffer, target)){
			m_searchIndex = 0;
			m_searchBuffer[0] = '\0';
			return WiFi::SRCH_SUCCEED;
		}

		if(String::strstr(m_searchBuffer,"ALREADY CONNECTED")){
			m_searchIndex = 0;
			m_searchBuffer[0] = '\0';
			return WiFi::SRCH_SUCCEED;
		}

		if(String::strstr(m_searchBuffer, "ERROR")){
			m_searchIndex = 0;
			m_searchBuffer[0] = '\0';
			return SRCH_FAILED;
		}

		if(String::strstr(m_searchBuffer, "FAIL")){
			m_searchIndex = 0;
			m_searchBuffer[0] = '\0';
			return SRCH_FAILED;
		}
	}

	if(String::strstr(target, "CLOSED")){
		//	If target is closed, server sends too much data so usr app usually
		//	does not pop rx data faster than isr pushes it (data is lost due to
		//	speed diff). In order to fix it, tiny blocking for (cpu speed)
		for(uint8_t index = 0; index < WiFi::MAX_SRCH_LEN; index++){
			int16_t data = m_wifiUart.receiveByte();

			if(data != -1){
				if(m_searchIndex < (WiFi::MAX_SRCH_LEN - 1)){
					m_searchBuffer[m_searchIndex++] = data;
					m_searchBuffer[m_searchIndex] = '\0';
				}else{
					for(uint8_t idx = 0; idx < (WiFi::MAX_SRCH_LEN / 2); idx++)
						m_searchBuffer[idx] = m_searchBuffer[(idx + (MAX_SRCH_LEN / 2))];

					m_searchIndex = (MAX_SRCH_LEN / 2);
					m_searchBuffer[m_searchIndex] = '\0';
				}
			}
		}
		if(String::strstr(m_searchBuffer, target, WiFi::MAX_SRCH_LEN)){
			m_searchIndex = 0;
			m_searchBuffer[0] = '\0';
			return WiFi::SRCH_SUCCEED;
		}
	}

	return 0;
}

void WiFi::buildAT(String &AT, wifiMode_t wifiMode, initStates_t initState){
	switch(initState){
		case initStates_t::iSEND_CREDENTIALS:
			switch(wifiMode){
				case wifiMode_t::WIFI_STATION_MODE:
					AT += "AT+CWJAP=\"";	//	AT+CWJAP=\"
					break;

				case wifiMode_t::WIFI_ACCESS_POINT_MODE:
					AT += "AT+CWSAP=\"";	//	AT+CWSAP=\"
					break;

				case wifiMode_t::WIFI_STATION_AND_ACCESS_POINT_MODE:
					AT += "AT+CWJAP=\"";	//	AT+CWJAP=\"
					break;

				default:
					//	Error
					break;
			}
			break;

		default:
			//	ERROR
			break;
	}
}

void WiFi::init(const char* ssid, const char* pass, wifiMode_t wifiMode){
	uint8_t searchAnswer;

	switch(m_initState){
		case initStates_t::iIDLE:{
			m_wifiInitializedFlag = false;
			m_wifiMode = wifiMode;
			String AT(m_ATcmdBuffer, WiFi::MAX_AT_CMD_LEN);
			AT += "AT+CWMODE=";
			AT += wifiMode;
			AT += "\r\n";

			if(AT.getError() == String::OK){
				m_wifiUart.sendStr(m_ATcmdBuffer);	//	Sends first byte
				m_initState = initStates_t::iWAIT_SETMODE;
			}
			break;
		}

		case initStates_t::iWAIT_SETMODE:
			if(m_wifiUart.sendStr(nullptr)){	//	Sends the remaining string
				m_initState = initStates_t::iWAIT_MODE_OK;
			}
			break;

		case initStates_t::iWAIT_MODE_OK:
			searchAnswer = WiFi::search4("OK");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_initState = initStates_t::iSEND_CREDENTIALS;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::IE_CWMODE;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iSEND_CREDENTIALS:{
			String AT(m_ATcmdBuffer, WiFi::MAX_AT_CMD_LEN);

			WiFi::buildAT(AT, m_wifiMode, iSEND_CREDENTIALS);

			AT += ssid;			//	AT+CWJAP=\"SSID
			AT += "\",\"";			//	AT+CWJAP=\"SSID\",\"
			AT += pass;			//	AT+CWJAP=\"SSID\",\"PASS
			AT += "\"\r\n";		//	AT+CWJAP=\"SSID\",\"PASS\r\n

			if(AT.getError() == String::OK){
				m_wifiUart.sendStr(m_ATcmdBuffer);	//	Sends first byte
				m_initState = initStates_t::iWAIT_SENDCRED;
			}else{
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;
		}

		case initStates_t::iWAIT_SENDCRED:
			if(m_wifiUart.sendStr(nullptr)){	//	Sends the remaining string
				m_initState = initStates_t::iWAIT_DISCONNECT;
			}
			break;

		case initStates_t::iWAIT_DISCONNECT:
			searchAnswer = WiFi::search4("DISCONNECT");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_initState = initStates_t::iWAIT_CONNECTED;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iWAIT_CONNECTED:
			searchAnswer = WiFi::search4("CONNECTED");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_initState = initStates_t::iWAIT_GOTIP;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iWAIT_GOTIP:
			searchAnswer = WiFi::search4("GOT IP");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_initState = initStates_t::iWAIT_IPOK;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iWAIT_IPOK:
			searchAnswer = WiFi::search4("OK");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_initState = initStates_t::iDONE;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iDONE:
			m_wifiInitializedFlag = true;
			break;

		case initStates_t::iERROR:
			switch(m_wifiError){
				case errors_t::IE_CWMODE:
					m_wifiError = errors_t::NONE;
					m_initState = initStates_t::iIDLE;
					break;

				case errors_t::IE_CWJAP:
					m_wifiError = errors_t::NONE;
					m_initState = initStates_t::iSEND_CREDENTIALS;
					break;

				case errors_t::NONE:
					break;

				default:
					break;
			}
			break;

		default:	//	ERROR
			break;
	}
}

bool WiFi::initFinished() const{ return m_wifiInitializedFlag; }

uint8_t WiFi::buildBody(char *data){
	String body(m_httpBody, WiFi::HTTP_MAX_BDY_LEN);

	body += "device=";
	body += m_httpUsrAgent;			//	m_httpBody = "device=[usrAgent]"
	body += "&path=";
	body += m_httpServerDataPath;	//	m_httpBody = "device=[usrAgent]&path=[serverDataPath]"
	body += "&data=";
	body += data;					//	m_httpBody = "device=[usrAgent]&path=[serverDataPath]&data=[data]"

	if(body.getError() == String::OK){
		m_httpBodyLen = body.getLen();
	}else{
		m_httpBody[0] = '\0';
		m_httpBodyLen = 0;
	}

	return m_httpBodyLen;
}

uint16_t WiFi::buildRequest(void){
	String request(m_httpRequest, WiFi::HTTP_MAX_RQST_LEN);

	request += "POST ";
	request += m_httpServerPath;
	request += " HTTP/1.1\r\n";
	request += "Host: ";
	request += m_httpServerHost;
	request += "\r\n";
	request += "Content-Type: application/x-www-form-urlencoded\r\n";
	request += "Content-Length: ";
	request += m_httpBodyLen;
	request += "\r\n";

	request += "Connection: close\r\n";
	request += "User-Agent: ";
	request += m_httpUsrAgent;
	request += "\r\n\r\n";
	request += m_httpBody;

	if(request.getError() == String::OK){
		m_httpRequestLen = request.getLen();
	}else{
		m_httpRequest[0] = '\0';
		m_httpRequestLen = 0;
	}

	return m_httpRequestLen;

/*	REQUEST:
 *  "POST [serverPath] HTTP/1.1\r\n"
 *  "Host: [serverHost]\r\n"
 *  "Content-Type: application/x-www-form-urlencoded\r\n"
 *  "Content-Length: [m_httpBodyLen]\r\n"
 *  "Connection: close\r\n"
 *  "User-Agent: [usrAgent]\r\n"
 *  "\r\n"
 *  "device=[usrAgent]&path=[serverDataPath]&data=[data]"
 */
}

bool WiFi::openConnection(){
	String AT(m_ATcmdBuffer, WiFi::MAX_AT_CMD_LEN);

	AT += "AT+CIPSTART=\"TCP\",\"";
	AT += m_httpServerHost;
	AT += "\",";
	AT += m_httpServerPort;
	AT += "\r\n";

	if(AT.getError() == String::OK){
		m_wifiUart.sendStr(m_ATcmdBuffer);	//	Sends first byte
		return true;
	}
	return false;
}

bool WiFi::buildRequestLen(){
	String AT(m_ATcmdBuffer, 30);

	AT += "AT+CIPSEND=";
	AT += m_httpRequestLen;
	AT += "\r\n";			//	Builds "AT+CIPSEND=m_httpRequestLen\r\n"

	if(AT.getError() == String::OK){
		m_wifiUart.sendStr(m_ATcmdBuffer);	//	Sends first byte
		return true;
	}
	return false;
}

bool WiFi::sendRequest(){
	if(m_wifiUart.sendStr(m_httpRequest)){
		return true;	//	All string sent
	}
	return false;
}

bool WiFi::closeConnection(){
	if(m_wifiUart.sendStr("AT+CIPCLOSE\r\n")){
		return true;	//	All string sent
	}
	return false;
}

void WiFi::uploadData(const char *serverDomain, uint16_t serverPort,  const char *serverPath, const char *serverDataPath, const char *device, char *data){
	uint8_t searchAnswer;

	switch(m_uploadState){
		case uploadStates_t::uIDLE:
			m_wifiUploadFinishedFlag = false;
			String::strcpy(m_httpServerHost, serverDomain);
			m_httpServerPort = serverPort;
			String::strcpy(m_httpServerPath, serverPath);
			String::strcpy(m_httpServerDataPath, serverDataPath);
			String::strcpy(m_httpUsrAgent, device);

			WiFi::buildBody(data);
			WiFi::buildRequest();

			if((m_httpBodyLen != 0) && (m_httpRequestLen != 0)){
				if(WiFi::openConnection()){
					m_uploadState = uploadStates_t::uWAIT_SENDOPEN;
				}
			}else{
				m_wifiError = errors_t::UE_CIPSTART;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_SENDOPEN:
			if(m_wifiUart.sendStr(nullptr)){	//	Sends the remaining string
				m_uploadState = uploadStates_t::uWAIT_CONNECT;
			}
			break;

		case uploadStates_t::uWAIT_CONNECT:
			searchAnswer = WiFi::search4("OK");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_uploadState = uploadStates_t::uSEND_LEN;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::UE_CIPSTART;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uSEND_LEN:
			if(WiFi::buildRequestLen()){
				m_uploadState = uploadStates_t::uWAIT_SENDLEN;
			}
			break;

		case uploadStates_t::uWAIT_SENDLEN:
			if(m_wifiUart.sendStr(nullptr)){	//	Sends the remaining string
				m_uploadState = uploadStates_t::uWAIT_LENOK;
			}
			break;

		case uploadStates_t::uWAIT_LENOK:
			searchAnswer = WiFi::search4("OK");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_uploadState = uploadStates_t::uWAIT_PROMPT;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::UE_CIPSEND;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_PROMPT:
			searchAnswer = WiFi::search4(">");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_uploadState = uploadStates_t::uSEND_REQUEST;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::UE_CIPSEND;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uSEND_REQUEST:
			if(WiFi::sendRequest()){
				m_uploadState = uploadStates_t::uWAIT_SENDOK;
			}
			break;

		case uploadStates_t::uWAIT_SENDOK:
			searchAnswer = WiFi::search4("SEND OK");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_uploadState = uploadStates_t::uWAIT_200OK;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::UE_SENDOK;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_200OK:
			searchAnswer = WiFi::search4("200 OK");
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_uploadState = uploadStates_t::uWAIT_CLOSED;
			}else if(searchAnswer == WiFi::SRCH_FAILED){
				m_wifiError = errors_t::UE_200OK;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_CLOSED:
			searchAnswer = WiFi::search4("CLOSED");	//	Sent by host
			if(searchAnswer == WiFi::SRCH_SUCCEED){
				m_uploadState = uploadStates_t::uDONE;
			}
			break;

		case uploadStates_t::uDONE:
			for(uint8_t i = 0; i < WiFi::MAX_SRCH_LEN; i++)	m_searchBuffer[i] = 0;
			m_uploadState = uploadStates_t::uIDLE;
			m_wifiUploadFinishedFlag = true;
			break;

		case uploadStates_t::uERROR:
			switch(m_wifiError){
				case errors_t::UE_CIPSTART:
					m_wifiError = errors_t::NONE;
					m_uploadState = uploadStates_t::uIDLE;
					break;

				case errors_t::UE_CIPSEND:
					if(m_wifiUart.sendStr("+++")){
						m_wifiError = errors_t::NONE;	//	All string sent
						m_uploadState = uploadStates_t::uIDLE;
					}
					break;

				case errors_t::UE_SENDOK:
					if(m_wifiUart.sendStr("+++")){
						m_wifiError = errors_t::NONE;	//	All string sent
						m_uploadState = uploadStates_t::uIDLE;
					}
					break;

				case errors_t::UE_200OK:
					m_wifiError = errors_t::NONE;
					m_uploadState = uploadStates_t::uIDLE;
					break;

				case errors_t::NONE:
					break;

				default:
					break;
			}
			break;

		default:	//	ERROR
			break;
	}
}

bool WiFi::uploadFinished(void) const{ return m_wifiUploadFinishedFlag; }

WiFi::~WiFi(){}
