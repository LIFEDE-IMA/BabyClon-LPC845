#include "statistics.h"
#include "mlx90614.h"
#include "ds18b20.h"
#include "wifi.h"
#include "eth.h"
#include "serial7segdisp.h"
#include "linealkeyboard.h"
#include "matrixkeyboard.h"
#include "hwinit.h"
#include "gpio.h"
#include "pint.h"
#include "systimer.h"
#include "digitalinput.h"
#include "adc.h"
#include "dac.h"
#include "i2c.h"
#include "ctimer.h"
#include "onewire.h"
#include "uart.h"
#include "spi.h"
#include "sctimer.h"
#include "string.h"
#include "led.h"
#include "bt.h"
#include "dma.h"

void heartbeat(void);
void dnsRetry(void);
void searchROM(void);
void convertT(void);
void readTemp(void);
void readMlxTemp(void);

static volatile bool f_heartbeatTimerExpired;
static volatile bool f_dnsRetryTimerExpired;
static volatile bool f_searchROMtimerExpired;
static volatile bool f_convertTempTimerExpired;
static volatile bool f_readTempTimerExpired;
static volatile bool f_readMlxTempTimerExpired;

int main(void){

	HW_init();
/*
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
*/
	SysTimer heartbeatTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, heartbeat);
	SysTimer dnsRetryTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, dnsRetry);
	SysTimer searchROMtimer(1, SysTimer::SINGLE, SysTimer::T_SEG, searchROM);
	SysTimer convertTempTimer(DS18B20::CONVERSION_TIME_MS, SysTimer::SINGLE, SysTimer::T_MILI, convertT);
	SysTimer readTempTimer(10, SysTimer::SINGLE, SysTimer::T_SEG, readTemp);
	SysTimer readMlxTempTimer(10, SysTimer::SINGLE, SysTimer::T_SEG, readMlxTemp);
/*
	Led ledR(0, 22);	//	External Red Led
	Led ledG(0, 21);	//	External Green Led
	Led ledB(0, 20);	//	External Blue Led
*/
	Led uLedR(1, 2, Gpio::AM_LOW);	//	Stick Red Led
	Led uLedG(1, 0, Gpio::AM_LOW);	//	Stick Green Led
	Led uLedB(1, 1, Gpio::AM_LOW);	//	Stick Blue Led
/*
	ledR.off();
	ledG.off();
	ledB.off();
*/
	uLedR.off();
	uLedG.off();
	uLedB.off();
	//uLedB.blink(500, Led::T_MILI);

	uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x15};

	char SERVER[] = "cifedegss.mooo.com";
	char SERVER_PATH[] = "/lab_server/guardar.php";
	char SERVER_DATA_PATH[] = "/!monitoringLPC_fullTest";
	char DEVICE[] = "monitoringLPC";
	char DATA_BUFF[145];
	String DATA(DATA_BUFF, 145);
	char SERVER_HEARTBEAT_PATH[] = "/lab_server/heartbeat.php";

	uint16_t serverPort = 8890;

	bool f_solvingDNS = false;
	bool f_retryingDNS = false;

	volatile uint32_t dnsRetries = 0;
	volatile uint32_t httpErrors = 0;

	bool f_uploadData = false;

	f_heartbeatTimerExpired = false;
	f_dnsRetryTimerExpired = false;
	f_searchROMtimerExpired = false;
	f_convertTempTimerExpired = false;
	f_readTempTimerExpired = false;
	f_readMlxTempTimerExpired = false;
/*
	eth.HTTPuploading(false);
	eth.HTTPheartBeating(false);

	eth.init(mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);

  	heartbeatTimer.startTimer();
	searchROMtimer.startTimer();
	readMlxTempTimer.startTimer();
*/
	float temps[3] = {0};
	float oneDecOwTemp1 = 0;
	float oneDecOwTemp2 = 0;
	enum{
		OW_IDLE,
		OW_SEARCH,
		OW_CONVERT,
		OW_CONVERTING,
		OW_WAIT,
		OW_READ_START,
		OW_READ_WAIT
	} owState = OW_IDLE;
	uint8_t slave = 0;
	uint8_t totalSlvs = 0;
	uint8_t measures = 0;

	float mlxTamb = 0;
	float mlxTobj = 0;
	float oneDecMlxTemp = 0;
	bool f_tempReading = false;

	bool f_updateDisplay = false;

	char readBT[20];
	char sendBT[48];
	String answBT(sendBT, 30);
	bool f_btReceived = false;
	bool f_btAnswering = false;
	bool f_btSendData = false;
	bool f_btSendingData = false;
	bool f_btNewTemps = false;

	bool f_uploadingWithWifi = false;
	bool f_uploadingWithEth = true;
	bool f_uploadChangeToWifiAsked = false;
	bool f_uploadChangeToEthAsked = false;

	const char SSID[] = "";
	const char PASS[] = "";

	bool f_initalizingWifi = true;
	bool f_wifiUploading = false;
	bool f_wifiHeartbeating = false;

	DmaTest dmaTest(22);
	dmaTest.init();

	bool f_dmaTestReported = false;
	volatile uint8_t passMask = 0;
	volatile uint8_t failMask = 0;

	DmaChainTest dmaChainTest(20, 21);
	dmaChainTest.init();	//	Dma::init() NOT Called Here (dmaTest.init() calls it)

	bool f_dmaChainTestReported = false;

	volatile uint8_t chainPassMask = 0;
	volatile uint8_t chainFailMask = 0;
	volatile uint8_t chainCopiedWords = 0;

	DmaTest2 dmaTest2(19);
	dmaTest2.init();	//	Dma::init() NOT Called Here (dmaTest.init() calls it)

	bool f_dmaTest2Reported = false;

	volatile uint8_t test2passMask = 0;
	volatile uint8_t test2failMask = 0;
	volatile uint8_t test2firstFailId = 0;
	volatile uint16_t test2abortCopied = 0;
	volatile uint16_t test2reuseFailIteration = 0;

    while(1){

/********************************************************
 *														*
 * 				     HC-05 BT MODULE					*
 * 														*
 ********************************************************/
/*
    	if(!f_btReceived && (bt.readData(readBT, 20) != Uart::NO_DATA_RECEIVED))
    		f_btReceived = true;

    	if(f_btReceived){
    		if(!f_btAnswering && !f_btSendingData){
    			f_btReceived = false;
    			if(String::strstr(readBT, ">YOU ALIVE?<")){
    				answBT = "YES";
    				f_btSendData = false;
    				f_btAnswering = true;
    			}
    			else if(String::strstr(readBT, ">GIVE ME DATA<")){
    				answBT = "OK, SENDING DATA";
    				f_btSendData = true;
    				f_btAnswering = true;
    			}
    			else if(String::strstr(readBT, ">STOP SENDING<")){
    				answBT = "OK, STOPPING";
    				f_btSendData = false;
    				f_btNewTemps = false;
    				f_btAnswering = true;
    			}
    			else if(String::strstr(readBT, ">UPLOAD W ETH<")){
    				answBT = "OK, UPLOADING W ETH";
    				f_btAnswering = true;
    				if(!f_uploadingWithEth){
    					f_uploadChangeToWifiAsked = false;
    					f_uploadChangeToEthAsked = true;
    				}
    			}
    			else if(String::strstr(readBT, ">UPLOAD W WIFI<")){
    				answBT = "OK, UPLOADING W WIFI";
    				f_btAnswering = true;
    				if(!f_uploadingWithWifi){
    					f_uploadChangeToWifiAsked = true;
    					f_uploadChangeToEthAsked = false;
    				}
    			}
    			else if(String::strstr(readBT, ">LED R<")){
					ledB.off();
					ledG.off();
					ledR.blink(500, Led::T_MILI);
				}
				else if(String::strstr(readBT, ">LED G<")){
					ledR.off();
					ledB.off();
					ledG.blink(500, Led::T_MILI);
				}
				else if(String::strstr(readBT, ">LED B<")){
					ledR.off();
					ledG.off();
					ledB.blink(500, Led::T_MILI);
				}
				else if(String::strstr(readBT,  ">LEDS OFF<")){
					ledR.off();
					ledG.off();
					ledB.off();
				}
    		}
    	}

    	if(f_btAnswering && !f_btSendingData){
    		if(bt.sendData(sendBT))
    			f_btAnswering = false;
    	}

    	if(f_btSendData && f_btNewTemps && !f_btAnswering && !f_btSendingData){	//	Display Updates Every Time Temp Changes
    		answBT = "T1=";
    		answBT += oneDecMlxTemp;
    		answBT += ", T2=";
    		answBT += oneDecOwTemp1;
    		answBT += ", T3=";
    		answBT += oneDecOwTemp2;

    		f_btNewTemps = false;
    		f_btSendingData = true;
    	}

    	if(f_btSendingData){
    		if(bt.sendData(sendBT))
    			f_btSendingData = false;
    	}
*/

/********************************************************
 *														*
 * 				   ONE-WIRE TEMP SENS					*
 * 														*
 ********************************************************/
/*
    	switch(owState){
			case OW_IDLE:
				if(f_searchROMtimerExpired){
		    		if(onewire.searchROM()){
						f_searchROMtimerExpired = false;
						searchROMtimer.stopTimer();
		    			owState = OW_SEARCH;
		    		}
				}
				break;

			case OW_SEARCH:
				if(!onewire.isBusy()){
					if(onewire.getStatus() == OneWire::OP_DONE){
						totalSlvs = onewire.getSlvsNumber();
		    			for(uint8_t slv = 0; slv < totalSlvs; slv++){
		    				dsTempSensors[slv]->setROM(onewire.getROM(slv));
		    			}
		    			readTempTimer.startTimer();
						owState = OW_CONVERT;
					}else{
						owState = OW_IDLE;
						searchROMtimer.startTimer();
					}
				}
				break;

			case OW_CONVERT:
				if(f_readTempTimerExpired){
					if(DS18B20::startTempConversion(onewire)){
						f_readTempTimerExpired = false;
						readTempTimer.stopTimer();
						owState = OW_CONVERTING;
					}
				}
				break;

			case OW_CONVERTING:
				if(!onewire.isBusy()){
					if(onewire.getStatus() == OneWire::OP_DONE){
						convertTempTimer.startTimer();
						owState = OW_WAIT;
					}else{
						owState = OW_IDLE;	//	Nobody Answered
						searchROMtimer.startTimer();
					}
				}
				break;

			case OW_WAIT:
				if(f_convertTempTimerExpired){
					f_convertTempTimerExpired = false;
					convertTempTimer.stopTimer();
					slave = 0;
					owState = OW_READ_START;
				}
				break;

			case OW_READ_START:
				if(dsTempSensors[slave]->startTempReading())
					owState = OW_READ_WAIT;
				break;

			case OW_READ_WAIT:
				dsTempSensors[slave]->processData();
				if(dsTempSensors[slave]->tempRdy() || dsTempSensors[slave]->hasError()){
					temps[slave] = -1.0f;
					if(dsTempSensors[slave]->tempRdy())
						temps[slave] = dsTempSensors[slave]->getTemp();
					slave++;
					if(slave < totalSlvs){
						owState = OW_READ_START;
					}else{
		        		oneDecOwTemp1 = ((int)(temps[0] * 10) / 10.f);
		        		oneDecOwTemp2 = ((int)(temps[1] * 10) / 10.f);
						measures++;
						if(measures >= 200){
							owState = OW_IDLE;
							searchROMtimer.startTimer();
						}else{
							readTempTimer.startTimer();
							owState = OW_CONVERT;
						}
					}
				}
				break;

			default:
				//	ERROR
				break;
    	}

*/
/********************************************************
 *														*
 * 				   I2C TEMP SENSOR						*
 * 														*
 ********************************************************/
/*
    	mlx90614.stateMachine();

    	if(!f_tempReading && f_readMlxTempTimerExpired){
    		f_readMlxTempTimerExpired = false;
    		readMlxTempTimer.stopTimer();

    		if(mlx90614.readFullTemp())
    			f_tempReading = true;
    	}

    	if(f_tempReading && !mlx90614.isBusy()){
    		f_tempReading = false;

    		if(mlx90614.isDone()){
    	    	mlxTamb = mlx90614.getTamb();
    	    	mlxTobj = mlx90614.getTobj();
        		oneDecMlxTemp = ((int)(mlxTobj * 10) / 10.f);
        		temps[2] = mlxTobj;
    	    	f_updateDisplay = true;
    	    	f_btNewTemps = true;
    	    	f_uploadData = true;
    	    	readMlxTempTimer.startTimer();
    		}else if(mlx90614.hasError()){
    			MLX90614::mlxStatus_t status = mlx90614.getStatus();
    		}
    	}

*/
/********************************************************
 *														*
 * 					 SERIAL DISPLAY						*
 * 														*
 ********************************************************/
/*
    	if(f_updateDisplay){
    		f_updateDisplay = false;
    	    display.setValue(oneDecMlxTemp);
    	}
*/

/********************************************************
 *														*
 * 					ETHERNET W5500						*
 * 														*
 ********************************************************/
/*
    	if(f_uploadingWithEth){
			eth.stateMachine();

			if(eth.isReady() && !f_solvingDNS){
				eth.DNSresolve(SERVER);
				f_solvingDNS = true;
			}

			if(eth.isReady() && f_solvingDNS && !f_retryingDNS && !eth.DNSresolveFinished()){
				f_retryingDNS = true;
				dnsRetries++;
				dnsRetryTimer.startTimer();
			}

			if(f_dnsRetryTimerExpired){
				f_dnsRetryTimerExpired = false;
				dnsRetryTimer.stopTimer();
				f_retryingDNS = false;
				f_solvingDNS = false;
			}

			if(!eth.HTTPisBusy() && f_heartbeatTimerExpired){
				eth.HTTPheartbeat(serverPort, SERVER_HEARTBEAT_PATH, DEVICE);
				f_heartbeatTimerExpired = false;
				eth.HTTPheartBeating(true);
				heartbeatTimer.stopTimer();
			}

			if(eth.HTTPheartbeatFinished() && !heartbeatTimer.isRunning()){
				eth.HTTPheartBeating(false);
				heartbeatTimer.startTimer();
			}

			if(!eth.HTTPisBusy() && f_uploadData){
				DATA = "T=";
				DATA += Statistics::median(temps, 3);
				DATA += ", T1=";
				DATA += temps[2];
				DATA += ", T2=";
				DATA += temps[1];
				DATA += ", T3=";
				DATA += temps[0];
				DATA += " ~~ DNS RTX: ";
				DATA += dnsRetries;
				DATA += ", HTTP ERR: ";
				DATA += httpErrors;

				eth.HTTPuploadData(serverPort, SERVER_PATH, SERVER_DATA_PATH, DEVICE, DATA.getStr());
				f_uploadData = false;
				eth.HTTPuploading(true);
			}

			if(eth.HTTPdataUploaded() && !f_uploadData){
				eth.HTTPuploading(false);

				if(f_uploadChangeToWifiAsked){
					if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
					f_uploadingWithEth = false;
					f_uploadingWithWifi = true;
					f_initalizingWifi = true;
				}
			}

			if(eth.HTTPerrorOccurred() && eth.isReady()){
				httpErrors++;
				f_uploadData = false;
				heartbeatTimer.stopTimer();
				eth.HTTPrestartAfterError();
				f_heartbeatTimerExpired = false;
				heartbeatTimer.startTimer();

				if(f_uploadChangeToWifiAsked){
					f_uploadingWithEth = false;
					f_uploadingWithWifi = true;
					f_initalizingWifi = true;
				}
			}
    	}
*/
/********************************************************
*														*
* 						WIFI ESP32						*
* 														*
********************************************************/
/*
    	if(f_uploadingWithWifi){
    		if(f_initalizingWifi){
        		wifi.init(SSID, PASS, WiFi::WIFI_STATION_MODE);

        		if(wifi.initFinished()){
        			f_initalizingWifi = false;
        		}

				if(f_uploadChangeToEthAsked){
					if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
					f_uploadingWithWifi = false;
					f_uploadingWithEth = true;
				}
    		}

			if(wifi.initFinished() && f_heartbeatTimerExpired){
				heartbeatTimer.stopTimer();
				f_heartbeatTimerExpired = false;
				f_wifiHeartbeating = true;
			}

			if(f_wifiHeartbeating)
				wifi.heartbeat(SERVER, serverPort, SERVER_HEARTBEAT_PATH, DEVICE);

			if(wifi.heartbeatFinished() && !heartbeatTimer.isRunning()){
				f_wifiHeartbeating = false;
				heartbeatTimer.startTimer();
			}

			if(wifi.initFinished() && f_uploadData && !f_wifiUploading){
				DATA = "T=";
				DATA += Statistics::median(temps, 3);
				DATA += ", T1=";
				DATA += temps[2];
				DATA += ", T2=";
				DATA += temps[1];
				DATA += ", T3=";
				DATA += temps[0];
				DATA += " ~~ WIFI MODE";

				f_wifiUploading = true;
			}

			if(f_wifiUploading){
				wifi.uploadData(SERVER, serverPort, SERVER_PATH, SERVER_DATA_PATH, DEVICE, DATA.getStr());
				f_uploadData = false;
			}

			if(wifi.uploadFinished() && !f_uploadData){
				f_wifiUploading = false;
				if(f_uploadChangeToEthAsked){
					if(!heartbeatTimer.isRunning())	heartbeatTimer.startTimer();
					f_uploadingWithWifi = false;
					f_uploadingWithEth = true;
				}
			}
    	}
*/
/********************************************************
 *														*
 * 						DMA TEST						*
 * 														*
 ********************************************************/
    	dmaTest.stateMachine();

    	if(dmaTest.hasFinished() && !f_dmaTestReported){
    		f_dmaTestReported = true;
    		passMask = dmaTest.getPassMask();
    		failMask = dmaTest.getFailMask();
    		if(dmaTest.allPassed())
    			uLedG.on();
    		else
    			uLedR.on();
    	}

    	dmaChainTest.stateMachine();

    	if(dmaChainTest.hasFinished() && !f_dmaChainTestReported){
    		f_dmaChainTestReported = true;
    		chainPassMask = dmaChainTest.getPassMask();
    		chainFailMask = dmaChainTest.getFailMask();
    		chainCopiedWords = dmaChainTest.getCopiedWords();
    		if(!dmaChainTest.allPassed()){
    			uLedG.off();
    			uLedR.on();
    		}
    	}

    	dmaTest2.stateMachine();
    	//	PASS MASK:
    	/*	BIT	|				WHAT IS TESTED (== 1 Only if All Passed)
    	 * 	 0	|	T_LIMIT: count == 1 -> writes 1 byte, count = 1024 -> copies 1024 bytes, rejects counts <= 0, counts >= 1025
    	 * 	 1	|	T_WIDTH: 16-bit with INC1 and INC2, 32-bit with INC4, fixed destination, 8-bit with INC4 on destination
    	 * 	 2	|	T_LINEAR: 3 linked descriptors (8, 16 and 32 bits), not a ring
    	 *   3	|	T_ABORT: requestStop() test
    	 *   4	|	T_RESTART: same channel does normal transfer after abort
    	 *   5	|	T_REUSE: 1000 transfers on one channel, rotating width, increment and options
    	 *   6	|	T_PP2SINGLE: ping-pong -> stop -> single transfer on the same channel
    	 *
    	 *	CHECK dma.cpp -> DmaTest2, For More Details Regarding this Table
    	 */
    	if(dmaTest2.hasFinished() && !f_dmaTest2Reported){
    		f_dmaTest2Reported = true;
    		test2passMask = dmaTest2.getPassMask();
    		test2failMask = dmaTest2.getFailMask();
    		test2firstFailId = dmaTest2.getFirstFailId();
    		test2abortCopied = dmaTest2.getAbortCopied();
    		test2reuseFailIteration = dmaTest2.getReuseFailIteration();
    		if(!dmaTest2.allPassed()){
    			uLedG.off();
    			uLedR.on();
    		}
    	}

    }
    return 0 ;
}

void heartbeat(void){
	f_heartbeatTimerExpired = true;
}

void dnsRetry(void){
	f_dnsRetryTimerExpired = true;
}

void searchROM(void){
	f_searchROMtimerExpired = true;
}

void convertT(void){
	f_convertTempTimerExpired = true;
}

void readTemp(void){
	f_readTempTimerExpired = true;
}

void readMlxTemp(void){
	f_readMlxTempTimerExpired = true;
}
