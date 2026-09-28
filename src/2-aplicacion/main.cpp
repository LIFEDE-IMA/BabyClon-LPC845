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
#include "statistics.h"
#include "mlx90614.h"
#include "ds18b20.h"
#include "wifi.h"
#include "eth.h"
#include "serial7segdisp.h"
#include "linealkeyboard.h"
#include "matrixkeyboard.h"

void uploadData(void);
void heartbeat(void);
void dnsRetry(void);
void readTemp(void);

static bool f_uploadTimerExpired;
static bool f_heartbeatTimerExpired;
static bool f_dnsRetryTimerExpired;
static bool f_readTempTimerExpired;

int main(void){

	HW_init();

	Spi spiMaster(0, 22, 0, 21, 0, 26, Spi::SPI_NUMBER_0, 500000, 4);
	//Spi spiMaster1(0, 16, 0, 17, 0, 18, Spi::SPI_NUMBER_1, 500000, 4);

	Eth eth(0, 23, spiMaster);

	Serial7segDisp display(0, 14, spiMaster);

	//OneWire onewire(0, 23);

	SysTimer uploadDataTimer(30, SysTimer::SINGLE, SysTimer::T_SEG, uploadData);
	SysTimer heartbeatTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, heartbeat);
	SysTimer dnsRetryTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, dnsRetry);
	SysTimer readTempTimer(2, SysTimer::SINGLE, SysTimer::T_SEG, readTemp);

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
	f_readTempTimerExpired = false;

	eth.HTTPuploading(false);
	eth.HTTPheartBeating(false);

	eth.init(mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);

    uploadDataTimer.startTimer();
    heartbeatTimer.startTimer();
	readTempTimer.startTimer();

	float temp = 0;

/*	bool f_firstWriteStarted = false;
	bool f_finishOp = false;
*/
    while(1){
    	/*
    	if(f_readTempTimerExpired){
    		f_readTempTimerExpired = false;
    		readTempTimer.stopTimer();
    		onewire.resetBus();
    		//readTempTimer.startTimer();
    	}

    	if(f_busRestFinished && !f_firstWriteStarted && !f_finishOp){
    		if(onewire.writeByte(OneWire::CMD_SKIP_ROM)){
    			f_firstWriteStarted = true;
    		}
    	}

    	if(f_busRestFinished &&  f_cmdSent && !f_finishOp){
    		if(onewire.writeByte(0x44)){
    			f_finishOp = true;
    		}
    	}

    	if(f_finishOp && f_cmdSent){
    		uint8_t i = 0;
    	}
*/

/********************************************************
*														*
* 					DISPLAY SERIAL						*
* 														*
********************************************************/
    	if(f_readTempTimerExpired){
    		f_readTempTimerExpired = false;
    	    readTempTimer.stopTimer();
    	    temp++;

    	    display.setValue(temp);

        	readTempTimer.startTimer();
    	}


/********************************************************
 *														*
 * 					ETHERNET W5500						*
 * 														*
 ********************************************************/

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

void readTemp(void){
	f_readTempTimerExpired = true;
}
