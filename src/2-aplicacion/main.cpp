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

void uploadData(void);
void heartbeat(void);
void dnsRetry(void);
void searchROM(void);
void convertT(void);
void readTemp(void);
void updateDisplay(void);

static bool f_uploadTimerExpired;
static bool f_heartbeatTimerExpired;
static bool f_dnsRetryTimerExpired;
static bool f_searchROMtimerExpired;
static bool f_convertTempTimerExpired;
static bool f_readTempTimerExpired;
static bool f_updateDisplayTimerExpired;

int main(void){

	HW_init();

	//Spi spiMaster(0, 4, 0, 6, 0, 0, Spi::SPI_NUMBER_0, 500000, 4);

	//Eth eth(0, 1, spiMaster);

	//Serial7segDisp display(0, 15, spiMaster);

	OneWire onewire(0, 23);
	DS18B20 ds18b20_1(onewire);	//	No ROM Yet
	DS18B20 ds18b20_2(onewire);	//	No ROM Yet
	DS18B20 *dsTempSensors[2] = {&ds18b20_1, &ds18b20_2};

	SysTimer uploadDataTimer(30, SysTimer::SINGLE, SysTimer::T_SEG, uploadData);
	SysTimer heartbeatTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, heartbeat);
	SysTimer dnsRetryTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, dnsRetry);
	SysTimer searchROMtimer(1, SysTimer::SINGLE, SysTimer::T_SEG, searchROM);
	SysTimer convertTempTimer(DS18B20::CONVERSION_TIME_MS, SysTimer::SINGLE, SysTimer::T_MILI, convertT);
	SysTimer readTempTimer(3, SysTimer::SINGLE, SysTimer::T_SEG, readTemp);
	SysTimer updateDisplayTimer(2, SysTimer::STRING, SysTimer::T_SEG, updateDisplay);

	Gpio ledG(1, 0, Gpio::D_OUTPUT, Gpio::AM_LOW);
	Gpio ledB(1, 1, Gpio::D_OUTPUT, Gpio::AM_LOW);
	Gpio ledR(1, 2, Gpio::D_OUTPUT, Gpio::AM_LOW);

	ledG.clrPin();
	ledB.clrPin();
	ledR.setPin();

	uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x32};

	char SERVER[] = "cifedegss.mooo.com";
	char SERVER_PATH[] = "/lab_server/guardar.php";
	char SERVER_DATA_PATH[] = "/$monitoringLPC_fullTest";
	char DEVICE[] = "monitoringLPC";
	char DATA_BUFF[145];
	String DATA(DATA_BUFF, 145);
	char SERVER_HEARTBEAT_PATH[] = "/lab_server/heartbeat.php";

	uint16_t serverPort = 8890;

	bool f_solvingDNS = false;
	bool f_retryingDNS = false;

	volatile uint32_t dnsRetries = 0;
	volatile uint32_t httpErrors = 0;

	f_uploadTimerExpired = false;
	f_heartbeatTimerExpired = false;
	f_dnsRetryTimerExpired = false;
	f_searchROMtimerExpired = false;
	f_convertTempTimerExpired = false;
	f_readTempTimerExpired = false;
	f_updateDisplayTimerExpired = false;

/*
	eth.HTTPuploading(false);
	eth.HTTPheartBeating(false);

	eth.init(mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);
*/
    uploadDataTimer.startTimer();
  	heartbeatTimer.startTimer();
	updateDisplayTimer.startTimer();
	searchROMtimer.startTimer();
	readTempTimer.startTimer();

	float temps[2] = {0};
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

    while(1){

/********************************************************
 *														*
 * 				   ONE-WIRE TEMP SENS					*
 * 														*
 ********************************************************/

    	switch(owState){
			case OW_IDLE:
				if(f_searchROMtimerExpired){
					f_searchROMtimerExpired = false;
					searchROMtimer.stopTimer();
		    		if(onewire.searchROM())
		    			owState = OW_SEARCH;
				}
				break;

			case OW_SEARCH:
				if(!onewire.isBusy()){
					if(onewire.getStatus() == OneWire::OP_DONE){
						totalSlvs = onewire.getSlvsNumber();
		    			for(uint8_t slv = 0; slv < totalSlvs; slv++){
		    				dsTempSensors[slv]->setROM(onewire.getROM(slv));
		    			}
						owState = OW_CONVERT;
					}else{
						owState = OW_IDLE;
					}
				}
				break;

			case OW_CONVERT:
				if(DS18B20::startTempConversion(onewire))
					owState = OW_CONVERTING;
				break;

			case OW_CONVERTING:
				if(!onewire.isBusy()){
					if(onewire.getStatus() == OneWire::OP_DONE){
						convertTempTimer.startTimer();
						owState = OW_WAIT;
					}else{
						owState = OW_IDLE;	//	Nobody Answered
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
					owState = (slave < totalSlvs) ? OW_READ_START : OW_IDLE;
				}
				break;

			default:
				//	ERROR
				break;
    	}


/********************************************************
 *														*
 * 				   I2C TEMP SENSOR						*
 * 														*
 ********************************************************/




/********************************************************
 *														*
 * 					 SERIAL DISPLAY						*
 * 														*
 ********************************************************/
/*
    	if(f_updateDisplayTimerExpired){
    		f_updateDisplayTimerExpired = false;
    	    temp++;

    	    display.setValue(temp);
    	}
*/

/********************************************************
 *														*
 * 					ETHERNET W5500						*
 * 														*
 ********************************************************/
/*
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
    		ledR.clrPin();
    		ledG.clrPin();
    		ledB.setPin();
    		eth.HTTPheartbeat(serverPort, SERVER_HEARTBEAT_PATH, DEVICE);
    		f_heartbeatTimerExpired = false;
    		eth.HTTPheartBeating(true);
    		heartbeatTimer.stopTimer();
    	}

    	if(eth.HTTPheartbeatFinished() && !heartbeatTimer.isRunning()){
    		eth.HTTPheartBeating(false);
    		heartbeatTimer.startTimer();
    	}

    	if(!eth.HTTPisBusy() && f_uploadTimerExpired){
    		ledR.clrPin();
    		ledB.clrPin();
    		ledG.setPin();

    		DATA = "Temp = ";
    		DATA += temp;
    		DATA += " ; en LIFEDE, via Ethernet. DNS RTX: ";
    		DATA += dnsRetries;
    		DATA += ", HTTP ERR: ";
    		DATA += httpErrors;

    		eth.HTTPuploadData(serverPort, SERVER_PATH, SERVER_DATA_PATH, DEVICE, DATA.getStr());
    		f_uploadTimerExpired = false;
    		eth.HTTPuploading(true);
    		uploadDataTimer.stopTimer();
    	}

    	if(eth.HTTPdataUploaded() && !uploadDataTimer.isRunning()){
    		eth.HTTPuploading(false);
    		uploadDataTimer.startTimer();
    	}

    	if(eth.HTTPerrorOccurred() && eth.isReady()){
    		ledB.clrPin();
    		ledG.clrPin();
    		ledR.setPin();
    		httpErrors++;
    		uploadDataTimer.stopTimer();
    		heartbeatTimer.stopTimer();
    		eth.HTTPrestartAfterError();
    		f_uploadTimerExpired = false;
    		f_heartbeatTimerExpired = false;
    		uploadDataTimer.startTimer();
    		heartbeatTimer.startTimer();
    	}
*/

/********************************************************
 *														*
 * 						LED RGB							*
 * 														*
 ********************************************************/




    }
    return 0 ;
}

void uploadData(void){
	f_uploadTimerExpired = true;
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

void updateDisplay(void){
	f_updateDisplayTimerExpired = true;
}
