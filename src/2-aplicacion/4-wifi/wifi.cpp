/*
 * wifi.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include <4-wifi/wifi.h>

WiFi::WiFi(Uart &uart) : m_wUart(uart){
	m_initState = initStates_t::iIDLE;
	m_uploadState = uploadStates_t::uIDLE;
	m_wifiError = errors_t::NONE;
}

uint8_t WiFi::search4(const char* target){
	static char buffer[MAX_RX_LEN];
	static uint16_t index = 0;
	uint8_t data = 0;

	if(m_wUart.popRx(&data)){
		if(index < MAX_RX_LEN - 1){
			buffer[index++] = data;
			buffer[index] = '\0';
		}else{
			index = 0;
		}
		if(String::strstr(buffer, target)){
			index = 0;
			buffer[0] = '\0';
			return 1;
		}
		if(String::strstr(buffer,"ALREADY CONNECTED")){
			return 1;
		}
		if(String::strstr(buffer, "ERROR")){
			return 2;
		}
	}
	return 0;
}

bool WiFi::init(const char* ssid, const char* pass){
	uint8_t success;

	switch(m_initState){
		case initStates_t::iIDLE:
			m_wUart.sendStr("AT+CWMODE=1\r\n");
			m_initState = initStates_t::iWAIT_SETMODE;
			break;

		case initStates_t::iWAIT_SETMODE:
			if(m_wUart.sendStr(nullptr)){
				m_initState = initStates_t::iWAIT_MODE_OK;
			}
			break;

		case initStates_t::iWAIT_MODE_OK:
			success = WiFi::search4("OK");
			if(success == YES){
				m_initState = initStates_t::iSEND_CREDENTIALS;
			}else if(success == NO){
				m_wifiError = errors_t::IE_CWMODE;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iSEND_CREDENTIALS:{
			char buffer[MAX_CMD_LEN];
			String cmd(buffer, MAX_CMD_LEN);

			cmd += "AT+CWJAP=\"";	//	AT+CWJAP=\"
			cmd += ssid;			//	AT+CWJAP=\"SSID
			cmd += "\",\"";			//	AT+CWJAP=\"SSID\",\"
			cmd += pass;			//	AT+CWJAP=\"SSID\",\"PASS
			cmd += "\"\r\n";		//	AT+CWJAP=\"SSID\",\"PASS\r\n

			if(cmd.getError() == String::OK){
				m_wUart.sendStr(cmd.getStr());
				m_initState = initStates_t::iWAIT_SENDCRED;
			}else{
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;
		}

		case initStates_t::iWAIT_SENDCRED:
			if(m_wUart.sendStr(nullptr)){
				m_initState = initStates_t::iWAIT_DISCONNECT;
			}
			break;

		case initStates_t::iWAIT_DISCONNECT:
			success = WiFi::search4("DISCONNECT");
			if(success == YES){
				m_initState = initStates_t::iWAIT_CONNECTED;
			}else if(success == NO){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iWAIT_CONNECTED:
			success = WiFi::search4("WIFI CONNECTED\r\n");
			if(success == YES){
				m_initState = initStates_t::iWAIT_GOTIP;
			}else if(success == NO){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iWAIT_GOTIP:
			success = WiFi::search4("GOT IP");
			if(success == YES){
				m_initState = initStates_t::iWAIT_IPOK;
			}else if(success == NO){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iWAIT_IPOK:
			success = WiFi::search4("OK");
			if(success == YES){
				m_initState = initStates_t::iDONE;
			}else if(success == NO){
				m_wifiError = errors_t::IE_CWJAP;
				m_initState = initStates_t::iERROR;
			}
			break;

		case initStates_t::iDONE:
			return true;
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
			return false;
			break;

		default:	//	ERROR
			return false;
			break;
	}
	return false;
}

uint8_t WiFi::makeBody(const char *device, const char *path, char *data){
	String body(m_httpBody, MAX_BDY_LEN);

	body += "device=";
	body += device;		//	m_httpBody = "device=[device]"
	body += "&path=";
	body += path;		//	m_httpBody = "device=[device]&path=[path]"
	body += "&data=";
	body += data;		//	m_httpBody = "device=[device]&path=[path]&data=[data]"

	if(body.getError() == String::OK){
		m_httpBodyLen = body.getLen();
	}else{
		m_httpBody[0] = '\0';
		m_httpBodyLen = 0;
	}

	return m_httpBodyLen;
}

uint16_t WiFi::makeRequest(const char *device){
	String request(m_httpRequest, MAX_RQST_LEN);

	request += "POST /lab_server/guardar.php HTTP/1.1\r\n";
	request += "Host: cifedegss.mooo.com\r\n";
	request += "Content-Type: application/x-www-form-urlencoded\r\n";
	request += "Content-Length: ";
	request += m_httpBodyLen;
	request += "\r\n";

	request += "Connection: close\r\n";
	request += "User-Agent: ";
	request += device;
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
 *  "POST /lab_server/guardar.php HTTP/1.1\r\n"
 *  "Host: cifedegss.mooo.com\r\n"
 *  "Content-Type: application/x-www-form-urlencoded\r\n"
 *  "Content-Length: m_httpBodyLen\r\n"
 *  "Connection: close\r\n"
 *  "User-Agent: [device]\r\n"
 *  "\r\n"
 *  "device=[device]&path=[path]&data=[data]"
 */
}

void WiFi::openConnection(){
	m_wUart.sendStr("AT+CIPSTART=\"TCP\",\"cifedegss.mooo.com\",8890\r\n");

	//	Wait for server answer
}

bool WiFi::buildRequestLen(){
	String cmd(m_cmdBuffer, 30);

	cmd += "AT+CIPSEND=";
	cmd += m_httpRequestLen;
	cmd += "\r\n";

	if(cmd.getError() == String::OK){
		return true;	//	Builds "AT+CIPSEND=m_httpRequestLen\r\n"
	}
	return false;
	//	Wait for server answer
}

void WiFi::sendRequest(){
	m_wUart.sendStr(m_httpRequest);

	//	Wait for server answer
}

void WiFi::closeConnection(){
	m_wUart.sendStr("AT+CIPCLOSE\r\n");

	//	Wait for server answer
}

bool WiFi::uploadData(const char *device, const char *path, char *data){
	uint8_t success;

	switch(m_uploadState){
		case uploadStates_t::uIDLE:{
			if(!(WiFi::makeBody(device, path, data))){
				return false;
			}
			if(!(WiFi::makeRequest(device))){
				return false;
			}

			WiFi::openConnection();
			m_uploadState = uploadStates_t::uWAIT_SENDOPEN;
			break;
		}

		case uploadStates_t::uWAIT_SENDOPEN:
			if(m_wUart.sendStr(nullptr)){
				m_uploadState = uploadStates_t::uWAIT_CONNECT;
			}
			break;

		case uploadStates_t::uWAIT_CONNECT:
			success = WiFi::search4("OK");
			if(success == YES){
				m_uploadState = uploadStates_t::uSEND_LEN;
			}else if(success == NO){
				m_wifiError = errors_t::UE_CIPSTART;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uSEND_LEN:
			if(WiFi::buildRequestLen()){
				m_wUart.sendStr(m_cmdBuffer);
			}
			m_uploadState = uploadStates_t::uWAIT_SENDLEN;
			break;

		case uploadStates_t::uWAIT_SENDLEN:
			if(m_wUart.sendStr(nullptr)){
				m_uploadState = uploadStates_t::uWAIT_LENOK;
			}
			break;

		case uploadStates_t::uWAIT_LENOK:
			success = WiFi::search4("OK");
			if(success == YES){
				m_uploadState = uploadStates_t::uWAIT_PROMPT;
			}else if(success == NO){
				m_wifiError = errors_t::UE_CIPSEND;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_PROMPT:
			success = WiFi::search4(">");
			if(success == YES){
				m_uploadState = uploadStates_t::uSEND_REQUEST;
			}else if(success == NO){
				m_wifiError = errors_t::UE_CIPSEND;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uSEND_REQUEST:
			WiFi::sendRequest();
			m_uploadState = uploadStates_t::uWAIT_SENDRQST;
			break;

		case uploadStates_t::uWAIT_SENDRQST:
			if(m_wUart.sendStr(nullptr)){
				m_uploadState = uploadStates_t::uWAIT_SENDOK;
			}
			break;

		case uploadStates_t::uWAIT_SENDOK:
			success = WiFi::search4("SEND OK");
			if(success == YES){
				m_uploadState = uploadStates_t::uWAIT_200OK;
			}else if(success == NO){
				m_wifiError = errors_t::UE_SENDOK;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_200OK:
			success = WiFi::search4("200 OK");
			if(success == YES){
				m_uploadState = uploadStates_t::uWAIT_CLOSED;
			}else if(success == NO){
				m_wifiError = errors_t::UE_200OK;
				m_uploadState = uploadStates_t::uERROR;
			}
			break;

		case uploadStates_t::uWAIT_CLOSED:
			success = WiFi::search4("CLOSED");	//	Sent by host
			if(success == YES){
				m_uploadState = uploadStates_t::uDONE;
			}
			break;

		case uploadStates_t::uDONE:
			m_uploadState = uploadStates_t::uIDLE;
			return true;
			break;

		case uploadStates_t::uERROR:
			switch(m_wifiError){
				case errors_t::UE_CIPSTART:
					m_wifiError = errors_t::NONE;
					m_uploadState = uploadStates_t::uIDLE;
					break;

				case errors_t::UE_CIPSEND:
					m_wUart.sendStr("+++");
					m_wifiError = errors_t::NONE;
					m_uploadState = uploadStates_t::uIDLE;
					break;

				case errors_t::UE_SENDOK:
					m_wUart.sendStr("+++");
					m_wifiError = errors_t::NONE;
					m_uploadState = uploadStates_t::uIDLE;
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
			return false;
			break;

		default:	//	ERROR
			return false;
			break;
	}
	return false;
}

WiFi::~WiFi(){}
