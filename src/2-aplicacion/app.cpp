/*
 * app.cpp
 *
 *  Created on: 31 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This source was written to handle main.cpp app
 */

#include "app.h"


/*#######################################################################
  #								TIMER FLAGS								#
  #######################################################################*/

static volatile bool f_heartbeatTimerExpired;
static volatile bool f_dnsRetryTimerExpired;
static volatile bool f_searchROMtimerExpired;
static volatile bool f_convertTempTimerExpired;
static volatile bool f_readOwTempTimerExpired;
static volatile bool f_readMlxTempTimerExpired;


/*#######################################################################
  #							TIMER HANDLERS								#
  #######################################################################*/

void heartbeat_timerHandler(void){
	f_heartbeatTimerExpired = true;
}

void dnsRetry_timerHandler(void){
	f_dnsRetryTimerExpired = true;
}

void searchROM_timerHandler(void){
	f_searchROMtimerExpired = true;
}

void convertT_timerHandler(void){
	f_convertTempTimerExpired = true;
}

void readOwTemp_timerHandler(void){
	f_readOwTempTimerExpired = true;
}

void readMlxTemp_timerHandler(void){
	f_readMlxTempTimerExpired = true;
}


/*#######################################################################
  #							CLASS INSTANCES								#
  #######################################################################*/

Spi spiMaster(0, 4, 0, 6, 0, 0, Spi::SPI_NUMBER_0, 500000, 4);
Eth eth(0, 1, spiMaster);
Serial7segDisp display(0, 15, spiMaster);

OneWire onewire(0, 23);
DS18B20 ds18b20_1(onewire);	//	No ROM Yet
DS18B20 ds18b20_2(onewire);	//	No ROM Yet
DS18B20 *dsTempSensors[2] = {&ds18b20_1, &ds18b20_2};

I2C i2c0(0, 0, 11, 0, 10, 100000, 30);
MLX90614 mlx90614(i2c0);

Uart uart0(Uart::UART_0, 0, 14, 0, 13);
BT bt(uart0, '>', '<', true);

Uart uart1(Uart::UART_1, 0, 9, 0, 8, 115200, 12);
WiFi wifi(uart1);

SysTimer heartbeatTimer(10, SysTimer::SINGLE, SysTimer::T_SEG, heartbeat_timerHandler);
SysTimer dnsRetryTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, dnsRetry_timerHandler);
SysTimer searchROMtimer(1, SysTimer::SINGLE, SysTimer::T_SEG, searchROM_timerHandler);
SysTimer convertTempTimer(DS18B20::CONVERSION_TIME_MS, SysTimer::SINGLE, SysTimer::T_MILI, convertT_timerHandler);
SysTimer readOwTempTimer(30, SysTimer::SINGLE, SysTimer::T_SEG, readOwTemp_timerHandler);
SysTimer readMlxTempTimer(30, SysTimer::SINGLE, SysTimer::T_SEG, readMlxTemp_timerHandler);

Led ledR(0, 22);	//	External Red Led
Led ledG(0, 21);	//	External Green Led
Led ledB(0, 20);	//	External Blue Led

Led uLedR(1, 2, Gpio::AM_LOW);	//	Stick Red Led
Led uLedG(1, 0, Gpio::AM_LOW);	//	Stick Green Led
Led uLedB(1, 1, Gpio::AM_LOW);	//	Stick Blue Led


/*#######################################################################
  #							MAIN VARIABLES							#
  #######################################################################*/

//	--------------------------	Ethernet  --------------------------
static bool f_ledsEnabled = true;
static bool f_ledsTesting = false;

static uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x37, 0x21};

static char SERVER[] = "cifedegss.mooo.com";
static char SERVER_PATH[] = "/lab_server/guardar.php";
static char SERVER_DATA_PATH[] = "/!monitoringLPC_fullTest";
static char DEVICE[] = "monitoringLPC";
static char SERVER_HEARTBEAT_PATH[] = "/lab_server/heartbeat.php";
static char DATA_BUFF[145];
String DATA(DATA_BUFF, 145);

static uint16_t serverPort = 8890;

static volatile uint32_t dnsRetries = 0;
static volatile uint32_t httpErrors = 0;

static bool f_uploadData = false;

enum ethStates_t : uint8_t{
	ETH_IDLE = 0,
	ETH_RESOLVE_DNS,
	ETH_SOLVING_DNS,
	ETH_RETRYING_DNS,
	ETH_START_HEARTBEAT,
	ETH_FINISH_HEARTBEAT,
	ETH_START_UPLOAD,
	ETH_FINISH_UPLOAD,
	ETH_ERROR
};

static uint8_t consErrCounter = 0;	//	Consecutive Errors ( Resolve DNS Again )
static const uint8_t MAX_CONSECUTIVE_ERRORS = 5;

//	--------------------------	Temp Sensors  --------------------------
static float allTemps[3] = {0};
static float oneDecOwTemp1 = 0;
static float oneDecOwTemp2 = 0;
enum owStates_t : uint8_t{
	OW_IDLE = 0,
	OW_SEARCH,
	OW_CONVERT,
	OW_CONVERTING,
	OW_WAIT,
	OW_READ_START,
	OW_READ_WAIT
};
static uint8_t currentSlv = 0;
static uint8_t totalSlvs = 0;
static uint8_t currentOwMeasures = 0;	//	Just Execute SEARCH_ROM when currentOwMeasures > MAX_OW_MEASURES
static const uint8_t MAX_OW_MEASURES = 200;
static bool f_allOwTempsRdy = false;

static float mlxTamb = 0;
static float mlxTobj = 0;
static float oneDecMlxTemp = 0;
static bool f_mlxTempRdy = false;

enum mlxStates_t : uint8_t{
	MLX_IDLE = 0,
	MLX_READ_TEMP
};

//	--------------------------	Display	--------------------------
static bool f_updateDisplay = false;

//	--------------------------	Bluetooth	--------------------------
static char readBT[130];
static char sendBT[48];
String answBT(sendBT, 48);
static const char *arg = nullptr;

static bool f_btSendData = false;
static bool f_btSendingData = false;
static bool f_btNewTemps = false;
static bool f_btMlxSampleTimeChangeAsked = false;
static bool f_btOwSampleTimeChangeAsked = false;

static uint8_t btSampleTimeSetter = 5;
static SysTimer::timeBase_t btTimeBaseSetter = SysTimer::T_SEG;

static bool f_uploadingWithWifi = false;
static bool f_uploadingWithEth = true;
static bool f_uploadChangeToWifiAsked = false;
static bool f_uploadChangeToEthAsked = false;
static bool f_wifiCredentialsChangeAsked = false;

enum btStates_t : uint8_t{
	BT_IDLE = 0,
	BT_PROCESS_DATA,
	BT_ANSWER,
	BT_SEND_DATA
};

//	--------------------------	WiFi  --------------------------

static char SSID[33] = "";
static char PASS[64] = "";
String wifiSSID(SSID, 33);
String wifiPASS(PASS, 64);

enum wifiStates_t : uint8_t{
	WIFI_IDLE = 0,
	WIFI_INIT,
	WIFI_HEARTBEAT,
	WIFI_UPLOADING,
	WIFI_UPLOAD_FINISHED
};


/*#######################################################################
  #								MDE LINKERS								#
  #######################################################################*/

bool updateDisplay(void){ return f_updateDisplay; }

void setUpdateDisplay(void){ f_updateDisplay = true; }

void clearUpdateDisplay(void){ f_updateDisplay = false; }

void checkForNewTemps(void){
	if(f_allOwTempsRdy && f_mlxTempRdy){
		f_allOwTempsRdy = false;
		f_mlxTempRdy = false;

		newTemps();
		setUpdateDisplay();
		setUploadRdyFlag();
	}
}

bool allTempsRdy(void){ return f_btNewTemps; }

void newTemps(void){ f_btNewTemps = true; }

void noNewTemps(void){ f_btNewTemps = false; }

bool ethMode(void){ return f_uploadingWithEth; }

bool wifiMode(void){ return f_uploadingWithWifi; }

void setEthMode(void){
	f_uploadingWithWifi = false;
	f_uploadingWithEth = true;
}

void setWifiMode(void){
	f_uploadingWithEth = false;
	f_uploadingWithWifi = true;
}

bool dataRdyForUpload(void){ return f_uploadData; }

void setUploadRdyFlag(void){ f_uploadData = true; }

void clearUploadRdyFlag(void){ f_uploadData = false; }



/*#######################################################################
  #							APP IMPLEMENTATIONS							#
  #######################################################################*/

void app_init(void){
	ledR.off();		//	External Leds
	ledG.off();
	ledB.off();

	uLedR.off();	//	LPC845 Leds
	uLedG.off();
	uLedB.off();

	uLedB.blink(500, Led::T_MILI);

	f_heartbeatTimerExpired = false;	//	Timers Flags
	f_dnsRetryTimerExpired = false;
	f_searchROMtimerExpired = false;
	f_convertTempTimerExpired = false;
	f_readOwTempTimerExpired = false;
	f_readMlxTempTimerExpired = false;

	//	Ethernet Init
	eth.HTTPuploading(false);
	eth.HTTPheartBeating(false);

	eth.init(mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);

	//	Wifi Default SSID and PASS
	wifiSSID = "Lifede";
	wifiPASS = "Lifede2026";

	//	Start Timers
  	heartbeatTimer.startTimer();
	searchROMtimer.startTimer();
	readMlxTempTimer.startTimer();
}

void btMDE(void){
	static btStates_t btState = btStates_t::BT_IDLE;

   	switch(btState){
   		case btStates_t::BT_IDLE:
   		   	if(bt.readData(readBT, 128) != Uart::NO_DATA_RECEIVED)
   		   		btState = btStates_t::BT_PROCESS_DATA;
   		   	else if(f_btSendData)	//	Send Data if Asked
				btState = btStates_t::BT_SEND_DATA;
   			break;

   		case btStates_t::BT_PROCESS_DATA:
			if(String::strstr(readBT, ">YOU ALIVE?<")){
				answBT = "YES";

				btState = btStates_t::BT_ANSWER;
			}
			else if(String::strstr(readBT, ">GIVE ME DATA<")){
				answBT = "OK, SENDING DATA";

				f_btSendData = true;

				btState = btStates_t::BT_ANSWER;
			}
			else if(String::strstr(readBT, ">STOP SENDING<")){
				answBT = "OK, STOPPING";

				f_btSendData = false;

				noNewTemps();

				btState = btStates_t::BT_ANSWER;
			}
			else if(String::strstr(readBT, ">UPLOAD W ETH<")){
				answBT = "OK, UPLOADING W ETH";

				if(!ethMode()){
					f_uploadChangeToWifiAsked = false;
					f_uploadChangeToEthAsked = true;
				}

				btState = btStates_t::BT_ANSWER;
			}
			else if(String::strstr(readBT, ">UPLOAD W WIFI<")){
				answBT = "OK, UPLOADING W WIFI";

				if(!wifiMode()){
					f_uploadChangeToWifiAsked = true;
					f_uploadChangeToEthAsked = false;
				}

				btState = btStates_t::BT_ANSWER;
			}
			else if(String::strstr(readBT, ">CHANGE WIFI, SSID: ")){
				//	FORMAT: >CHANGE WIFI, SSID: [ssid], PASS: [pass]<
				const char *ssid = nullptr;
				const char *pass = nullptr;
				uint16_t ssidLen = 0;
				uint16_t passLen = 0;

				if(String::findBetween(readBT, ">CHANGE WIFI, SSID: ", ", PASS: ", ssid, ssidLen) &&	//	Gets Pointer to SubStr Between SSID: and PASS: and its Len
				   String::findBetween((ssid + ssidLen), ", PASS: ", "<", pass, passLen)		  &&
				   (ssidLen > 0) && (ssidLen <= wifiSSID.getCapacity()) && (passLen <= wifiPASS.getCapacity())){

					wifiSSID.assign(ssid, ssidLen);
					wifiPASS.assign(pass, passLen);

					f_wifiCredentialsChangeAsked = true;

					answBT = "OK, WIFI SAVED";
				}else{
					answBT = "ERR, BAD WIFI CREDS";
				}

				btState = btStates_t::BT_ANSWER;
			}
			else if(String::strstr(readBT, ">LED R<")){
				f_ledsTesting = true;

				ledB.off();
				ledG.off();
				ledR.blink(500, Led::T_MILI);

				btState = btStates_t::BT_IDLE;
			}
			else if(String::strstr(readBT, ">LED G<")){
				f_ledsTesting = true;

				ledR.off();
				ledB.off();
				ledG.blink(500, Led::T_MILI);

				btState = btStates_t::BT_IDLE;
			}
			else if(String::strstr(readBT, ">LED B<")){
				f_ledsTesting = true;

				ledR.off();
				ledG.off();
				ledB.blink(500, Led::T_MILI);

				btState = btStates_t::BT_IDLE;
			}
			else if(String::strstr(readBT,  ">LEDS OFF<")){
				f_ledsTesting = false;
				f_ledsEnabled = false;

				ledR.off();
				ledG.off();
				ledB.off();

				btState = btStates_t::BT_IDLE;
			}
			else if(String::strstr(readBT,  ">LEDS ON<")){
				f_ledsEnabled = true;

				btState = btStates_t::BT_IDLE;
			}
			else if((arg = String::findAfter(readBT, ">CHANGE SAMPLE TIME TO ")) != nullptr){	//	arg Points to Whatever is After "TO "
				//	FORMAT: >CHANGE SAMPLE TIME TO [1-255] [SEC | MIN]<
				const char *units[2] = {"SEC<", "MIN<"};
				const SysTimer::timeBase_t bases[2] = {SysTimer::T_SEG, SysTimer::T_MIN};

				uint32_t value = 0;
				const char *unit = String::str_to_uint(arg, value, 3);
				int unitIdx = -1;

				if(unit && (value >= 1) && (value <= 255) && (*unit == ' ')){
					unitIdx = String::matchPrefix((unit + 1), units, 2);	//	Skips Space, Searches units[] Into unit
				}

				if(unitIdx >= 0){
					btTimeBaseSetter = bases[unitIdx];
					btSampleTimeSetter = (uint8_t)value;
					f_btMlxSampleTimeChangeAsked = true;
					f_btOwSampleTimeChangeAsked = true;
					answBT = "OK, SAMPLE TIME SET";
				}else{
					answBT = "ERR, BAD SAMPLE TIME";
				}

				btState = btStates_t::BT_ANSWER;
			}
			else{
				btState = btStates_t::BT_IDLE;	//	Invalid CMD -> Discard
			}
   			break;

   		case btStates_t::BT_ANSWER:
   			if(bt.sendData(sendBT)){
   				if(f_btSendData)	//	Start Sending Data
   					btState = btStates_t::BT_SEND_DATA;
   				else	//	Nothing More To Do
   					btState = btStates_t::BT_IDLE;
   			}
   			break;

   		case btStates_t::BT_SEND_DATA:
   			if(allTempsRdy()){	//	Waits To Have New Temps
   				answBT = "T=";
   				answBT += Statistics::median(allTemps, 3);
   				answBT += ", T1=";
   				answBT += oneDecMlxTemp;
   				answBT += ", T2=";
   				answBT += oneDecOwTemp1;
   				answBT += ", T3=";
   				answBT += oneDecOwTemp2;

   				noNewTemps();

   				f_btSendingData = true;
   			}
   			else if(f_btSendingData){
   				if(bt.sendData(sendBT)){
   					f_btSendingData = false;
   					btState = btStates_t::BT_IDLE;
   				}
   			}else{
   				btState = btStates_t::BT_IDLE;
   			}
   			break;

   		default:
   			//	ERROR, Unblock Machine
   			f_btSendData = false;
   			f_btSendingData = false;
   			noNewTemps();
			f_btMlxSampleTimeChangeAsked = false;
			f_btOwSampleTimeChangeAsked = false;
			f_ledsEnabled = true;
			f_ledsTesting = false;
   			answBT = "";
			btState = btStates_t::BT_IDLE;
   			break;
   	}
}

void owMDE(void){
	static owStates_t owState = owStates_t::OW_IDLE;

	switch(owState){
		case owStates_t::OW_IDLE:
			if(f_searchROMtimerExpired){
	    		if(onewire.searchROM()){	//	Reset Bus and Start Search ROM Tree
					f_searchROMtimerExpired = false;
					searchROMtimer.stopTimer();
	    			owState = owStates_t::OW_SEARCH;
	    		}
			}
			break;

		case owStates_t::OW_SEARCH:
			if(!onewire.isBusy()){	//	Search ROM Finished or Aborted
				if(onewire.getStatus() == OneWire::OP_DONE){
					totalSlvs = onewire.getSlvsNumber();	//	Number of Slaves Found
	    			for(uint8_t slv = 0; slv < totalSlvs; slv++){
	    				dsTempSensors[slv]->setROM(onewire.getROM(slv));	//	Assign Found ROMs
	    			}
	    			readOwTempTimer.startTimer();	//	Starts Timer for First Read
					owState = owStates_t::OW_CONVERT;
				}else{
					owState = owStates_t::OW_IDLE;
					searchROMtimer.startTimer();	//	Try Again
				}
			}
			break;

		case owStates_t::OW_CONVERT:
			if(f_readOwTempTimerExpired){
				if(DS18B20::startTempConversion(onewire)){	//	All Slaves: Convert T
					f_readOwTempTimerExpired = false;
					f_allOwTempsRdy = false;
					readOwTempTimer.stopTimer();
					owState = owStates_t::OW_CONVERTING;
				}
			}
			break;

		case owStates_t::OW_CONVERTING:
			if(!onewire.isBusy()){	//	Finished Sending CMD_CONVERT_T or Failed
				if(onewire.getStatus() == OneWire::OP_DONE){
					convertTempTimer.startTimer();	//	Wait ~750ms for the Conversion to End
					owState = owStates_t::OW_WAIT;
				}else{
					owState = owStates_t::OW_IDLE;	//	Nobody Answered
					searchROMtimer.startTimer();
				}
			}
			break;

		case owStates_t::OW_WAIT:
			if(f_convertTempTimerExpired){
				f_convertTempTimerExpired = false;
				convertTempTimer.stopTimer();
				currentSlv = 0;	//	Next Op Starts with First Slave Found
				owState = owStates_t::OW_READ_START;
			}
			break;

		case owStates_t::OW_READ_START:
			if(dsTempSensors[currentSlv]->startTempReading())	//	Read Scratchpad from Current Slave
				owState = owStates_t::OW_READ_WAIT;
			break;

		case owStates_t::OW_READ_WAIT:
			dsTempSensors[currentSlv]->processData();	//	Read Raw Temp Data and Check CRC
			if(dsTempSensors[currentSlv]->tempRdy() || dsTempSensors[currentSlv]->hasError()){
				allTemps[currentSlv] = -255.0f;	//	Error Converting, Wrong CRC
				if(dsTempSensors[currentSlv]->tempRdy())
					allTemps[currentSlv] = dsTempSensors[currentSlv]->getTemp();	//	Temp in °C
				currentSlv++;	//	Move On to Next Slave
				if(currentSlv < totalSlvs){
					owState = owStates_t::OW_READ_START;	//	Repeat
				}else{	//	Finished With All Slaves
					f_allOwTempsRdy = true;

					oneDecOwTemp1 = ((int)(allTemps[0] * 10) / 10.f);	//	For BT App
					oneDecOwTemp2 = ((int)(allTemps[1] * 10) / 10.f);

					checkForNewTemps();

					currentOwMeasures++;

					if(currentOwMeasures >= MAX_OW_MEASURES){
						owState = owStates_t::OW_IDLE;
						searchROMtimer.startTimer();	//	SEARCH_ROM Only After MAX_OW_MEASURES
					}else{
						if(f_btOwSampleTimeChangeAsked){	//	App Asked Sample Time Change, Wait Till Current Sample Finishes to Change It
							readOwTempTimer.setTimer(btSampleTimeSetter, btTimeBaseSetter);
							f_btOwSampleTimeChangeAsked = false;
						}
						readOwTempTimer.startTimer();	//	Start Again
						owState = owStates_t::OW_CONVERT;
					}
				}
			}
			break;

		default:
			//	ERROR, Unblock Machine
			totalSlvs = 0;
			currentSlv = 0;
			currentOwMeasures = 0;
			readOwTempTimer.stopTimer();
			searchROMtimer.stopTimer();
			convertTempTimer.stopTimer();
			f_searchROMtimerExpired = false;
			f_readOwTempTimerExpired = false;
			f_convertTempTimerExpired = false;
			searchROMtimer.startTimer();
			owState = owStates_t::OW_IDLE;
			break;
	}
}

void mlxMDE(void){
	static mlxStates_t mlxState = mlxStates_t::MLX_IDLE;

	mlx90614.stateMachine();	//	Internal MDE (Low Level)

	switch(mlxState){
		case mlxStates_t::MLX_IDLE:
			if(f_readMlxTempTimerExpired){
				f_readMlxTempTimerExpired = false;
				f_mlxTempRdy = false;
				readMlxTempTimer.stopTimer();

				if(mlx90614.readFullTemp())
					mlxState = mlxStates_t::MLX_READ_TEMP;
				else
					readMlxTempTimer.startTimer();	//	Restart
			}
			break;

		case mlxStates_t::MLX_READ_TEMP:
			if(!mlx90614.isBusy()){
				if(mlx90614.isDone()){
			    	mlxTamb = mlx90614.getTamb();
			    	mlxTobj = mlx90614.getTobj();
		    		oneDecMlxTemp = ((int)(mlxTobj * 10) / 10.f);
		    		allTemps[2] = mlxTamb;	//	0 and 1 are MY18B20
		    		f_mlxTempRdy = true;

					checkForNewTemps();

					if(f_btMlxSampleTimeChangeAsked){	//	App Asked Sample Time Change, Wait Till Current Sample Finishes to Change It
						readMlxTempTimer.setTimer(btSampleTimeSetter, btTimeBaseSetter);
						f_btMlxSampleTimeChangeAsked = false;
					}

			    	readMlxTempTimer.startTimer();
				}else if(mlx90614.hasError()){
					MLX90614::mlxStatus_t status = mlx90614.getStatus();
				}

				mlxState = mlxStates_t::MLX_IDLE;
			}
			break;

		default:
			//	ERROR, Unblock MDE
			readMlxTempTimer.stopTimer();
			f_readMlxTempTimerExpired = false;
			readMlxTempTimer.startTimer();
			mlxState = mlxStates_t::MLX_IDLE;
			break;
	}
}

void ethMDE(void){
	static ethStates_t ethState = ethStates_t::ETH_RESOLVE_DNS;

	if(!ethMode())	return;	//	WiFi Mode Set

	eth.stateMachine();	//	Internal MDE (Low Level)

	if(eth.HTTPerrorOccurred() && eth.isReady())	//	Errors Handler
		ethState = ethStates_t::ETH_ERROR;

	switch(ethState){
		case ethStates_t::ETH_IDLE:
			if(!eth.HTTPisBusy() && f_heartbeatTimerExpired)
				ethState = ethStates_t::ETH_START_HEARTBEAT;
			else if(!eth.HTTPisBusy() && dataRdyForUpload())
				ethState = ethStates_t::ETH_START_UPLOAD;
			break;

		case ethStates_t::ETH_RESOLVE_DNS:
			if(eth.isReady()){
				eth.DNSresolve(SERVER);	//	Get Server IP
				ethState = ethStates_t::ETH_SOLVING_DNS;
			}
			break;

		case ethStates_t::ETH_SOLVING_DNS:
			if(eth.isReady()){
				if(eth.DNSresolveFinished()){
					ethState = ethStates_t::ETH_IDLE;
				}else{	//	Bad DNS Resolution
					dnsRetries++;
					dnsRetryTimer.startTimer();
					ethState = ethStates_t::ETH_RETRYING_DNS;
				}
			}
			break;

		case ethStates_t::ETH_RETRYING_DNS:
			if(f_dnsRetryTimerExpired){
				f_dnsRetryTimerExpired = false;
				dnsRetryTimer.stopTimer();
				ethState = ethStates_t::ETH_RESOLVE_DNS;	//	Resolve DNS Again
			}
			break;

		case ethStates_t::ETH_START_HEARTBEAT:
			eth.HTTPheartbeat(serverPort, SERVER_HEARTBEAT_PATH, DEVICE);
			f_heartbeatTimerExpired = false;
			eth.HTTPheartBeating(true);
			heartbeatTimer.stopTimer();
			ethState = ethStates_t::ETH_FINISH_HEARTBEAT;
			break;

		case ethStates_t::ETH_FINISH_HEARTBEAT:
			if(eth.HTTPheartbeatFinished() && !heartbeatTimer.isRunning()){
				eth.HTTPheartBeating(false);
				heartbeatTimer.startTimer();
				ethState = ethStates_t::ETH_IDLE;
			}
			break;

		case ethStates_t::ETH_START_UPLOAD:
			DATA = "T=";
			DATA += Statistics::median(allTemps, 3);
			DATA += ", Tmlx=";
			DATA += allTemps[2];	//	MLX
			DATA += ", Tow1=";
			DATA += allTemps[1];	//	MY18B20
			DATA += ", Tow2=";
			DATA += allTemps[0];	//	MY18B20
			DATA += " ~~ DNS RTX: ";
			DATA += dnsRetries;
			DATA += ", HTTP ERR: ";
			DATA += httpErrors;

			if(f_ledsEnabled && !f_ledsTesting){	//	External Leds
				ledR.off();
				ledG.off();
				ledB.blink(500, Led::T_MILI);
			}

			eth.HTTPuploadData(serverPort, SERVER_PATH, SERVER_DATA_PATH, DEVICE, DATA.getStr());

			clearUploadRdyFlag();

			eth.HTTPuploading(true);
			ethState = ethStates_t::ETH_FINISH_UPLOAD;
			break;

		case ethStates_t::ETH_FINISH_UPLOAD:
			if(eth.HTTPdataUploaded()){
				if(f_ledsEnabled && !f_ledsTesting){	//	External Leds
					ledB.off();
					ledR.off();
					ledG.blink(500, Led::T_MILI);
				}

				eth.HTTPuploading(false);

				if(f_uploadChangeToWifiAsked){
					if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
					setWifiMode();
				}

				consErrCounter = 0;	//	Restart Consecutive Errors Counter

				ethState = ethStates_t::ETH_IDLE;
			}
			break;

		case ethStates_t::ETH_ERROR:
			if(f_ledsEnabled && !f_ledsTesting){	//	External Leds
				ledB.off();
				ledG.off();
				ledR.blink(500, Led::T_MILI);
			}

			httpErrors++;
			consErrCounter++;
			clearUploadRdyFlag();
			heartbeatTimer.stopTimer();
			eth.HTTPrestartAfterError();
			f_heartbeatTimerExpired = false;
			heartbeatTimer.startTimer();

			if(f_uploadChangeToWifiAsked){
				setWifiMode();
			}

			if(consErrCounter >= MAX_CONSECUTIVE_ERRORS){
				dnsRetries++;
				dnsRetryTimer.startTimer();
				ethState = ethStates_t::ETH_RETRYING_DNS;
			}else{
				ethState = ethStates_t::ETH_IDLE;
			}
			break;

		default:
			//	ERROR, Unblock MDE
			ethState = ethStates_t::ETH_ERROR;
			break;
	}
}

void wifiMDE(void){
	static wifiStates_t wifiState = wifiStates_t::WIFI_INIT;

	if(!wifiMode())	return;	//	Eth Mode Set

	switch(wifiState){
		case wifiStates_t::WIFI_INIT:
			wifi.init(wifiSSID.getStr(), wifiPASS.getStr(), WiFi::WIFI_STATION_MODE);	//	Init Internal State Machine

			if(wifi.initFinished()){
				if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
				wifiState = wifiStates_t::WIFI_IDLE;	//	Wait for Heartbeat or Upload Request
			}

			if(f_uploadChangeToEthAsked){
				if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
				setEthMode();
			}
			break;

		case wifiStates_t::WIFI_IDLE:
			if(wifi.initFinished() && f_heartbeatTimerExpired){
				heartbeatTimer.stopTimer();
				f_heartbeatTimerExpired = false;
				wifiState = wifiStates_t::WIFI_HEARTBEAT;
			}
			else if(wifi.initFinished() && dataRdyForUpload()){
				DATA = "T=";
				DATA += Statistics::median(allTemps, 3);
				DATA += ", Tmlx=";
				DATA += allTemps[2];	//	MLX90614
				DATA += ", Tow1=";
				DATA += allTemps[1];	//	MY18B20
				DATA += ", Tow2=";
				DATA += allTemps[0];	//	MY18B20
				DATA += " ~~ WIFI MODE";

				if(f_ledsEnabled && !f_ledsTesting){	//	External Leds
					ledR.off();
					ledG.off();
					ledB.blink(500, Led::T_MILI);
				}

				wifiState = wifiStates_t::WIFI_UPLOADING;
			}
			break;

		case wifiStates_t::WIFI_HEARTBEAT:
			wifi.heartbeat(SERVER, serverPort, SERVER_HEARTBEAT_PATH, DEVICE);	//	Internal State Machine

			if(wifi.heartbeatFinished()){
				heartbeatTimer.startTimer();
				wifiState = wifiStates_t::WIFI_IDLE;	//	Wait for Next Op Request
			}
			break;

		case wifiStates_t::WIFI_UPLOADING:
			wifi.uploadData(SERVER, serverPort, SERVER_PATH, SERVER_DATA_PATH, DEVICE, DATA.getStr());	//	Internal Handler
			if(dataRdyForUpload())	clearUploadRdyFlag();

			if(wifi.uploadFinished())
				wifiState = wifiStates_t::WIFI_UPLOAD_FINISHED;
			break;

		case wifiStates_t::WIFI_UPLOAD_FINISHED:
			if(f_ledsEnabled && !f_ledsTesting){	//	External Leds
				ledR.off();
				ledB.off();
				ledG.blink(500, Led::T_MILI);
			}

			if(f_uploadChangeToEthAsked){
				if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
				setEthMode();
				wifiState = wifiStates_t::WIFI_IDLE;	//	Wait for Next Op Request
			}
			else if(f_wifiCredentialsChangeAsked){
				if(heartbeatTimer.isRunning())	heartbeatTimer.stopTimer();
				f_wifiCredentialsChangeAsked = false;
				wifiState = wifiStates_t::WIFI_INIT;	//	Restart Init With New SSID and PASS
			}
			else{
				wifiState = wifiStates_t::WIFI_IDLE;	//	Wait for Next Op Request
			}
			break;

		default:
			//	ERROR, Unblock Machine
			f_uploadingWithWifi = true;
			f_uploadingWithEth = false;
			clearUploadRdyFlag();
			heartbeatTimer.stopTimer();
			f_heartbeatTimerExpired = false;
			heartbeatTimer.startTimer();
			wifiState = wifiStates_t::WIFI_INIT;
			break;
	}
}

void serialDisplay(void){
	if(updateDisplay()){
		clearUpdateDisplay();
	    display.setValue(Statistics::median(allTemps, 3));	//	Shows Median of the 3 Temps
	}
}
