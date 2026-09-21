#include "hwinit.h"
#include "statistics.h"
#include "mlx90614.h"
#include "ds18b20.h"
#include "wifi.h"
#include "gpio.h"
#include "pint.h"
#include "systick.h"
#include "systimer.h"
#include "digitalinput.h"
#include "adc.h"
#include "dac.h"
#include "uart.h"
#include "i2c.h"
#include "ctimer.h"
#include "onewire.h"
#include "spi.h"
#include "eth.h"
#include "sctimer.h"

void uploadData(void);
void heartbeat(void);
void dnsRetry(void);

static bool f_uploadTimerExpired;
static bool f_heartbeatTimerExpired;
static bool f_dnsRetryTimerExpired;

int main(void){

	HW_init();

	Spi spiMaster(0, 22, 0, 21, 0, 26, Spi::SPI_NUMBER_0, 1000000);
	Eth eth(0, 23, spiMaster);

	SysTimer uploadDataTimer(30, SysTimer::SINGLE, SysTimer::T_SEG, uploadData);
	SysTimer heartbeatTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, heartbeat);
	SysTimer dnsRetryTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, dnsRetry);

	Gpio ledG(1, 0, Gpio::D_OUTPUT, Gpio::AM_LOW);
	Gpio ledB(1, 1, Gpio::D_OUTPUT, Gpio::AM_LOW);
	Gpio ledR(1, 2, Gpio::D_OUTPUT, Gpio::AM_LOW);

	ledG.clrPin();
	ledB.clrPin();
	ledR.setPin();

	uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x32};

	char SERVER[] = "cifedegss.mooo.com";
	char SERVER_PATH[] = "/lab_server/guardar.php";
	char SERVER_DATA_PATH[] = "/$clonLPC_0_testEth";
	char DEVICE[] = "clonLPC_0";
	char DATA_BUFF[145];
	char SERVER_HEARTBEAT_PATH[] = "/lab_server/heartbeat.php";

	uint16_t serverPort = 8890;

	bool f_solvingDNS = false;
	bool f_retryingDNS = false;

	volatile uint32_t dnsRetries = 0;
	volatile uint32_t httpErrors = 0;

	f_uploadTimerExpired = false;
	f_heartbeatTimerExpired = false;
	f_dnsRetryTimerExpired = false;

	eth.HTTPuploading(false);
	eth.HTTPheartBeating(false);

    eth.init(mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);

    uploadDataTimer.startTimer();
    heartbeatTimer.startTimer();

/*
	SCTimer Sct(SCTimer::sctUNIFIED_MODE);
	Sct.configAutoLimit(SCTimer::UNIFIED_COUNTER, true);
	Sct.configRegisterMode(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_0, SCTimer::regMATCH_MODE);
	Sct.init();
	Sct.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0, SCTimer::sctREGISTER_0, SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT);
	Sct.enableStateEvent(SCTimer::sctEVENT_0, SCTimer::sctEVENT_STATE_0);
	Sct.enableEventInterrupt(SCTimer::sctEVENT_0);
	Sct.setTimer(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_0, 1, SCTimer::T_SEG);
	Sct.enableNVICint();
	Sct.startCounter(SCTimer::UNIFIED_COUNTER);
*/
    while(1){
/*
    	if(f_int){
    		ledG.togglePin();
    		f_int = false;
    	}
*/

    	//	ETH:

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


 /*   	///////////////////////////////////////////////////////////
    	if(!eth.HTTPisBusy() && f_heartbeatTimerExpired){
    		eth.socketRequestStatus();
    		f_heartbeatTimerExpired = false;
    		heartbeatTimer.stopTimer();
    	}

    	if(!eth.HTTPisBusy() && eth.socketStatusRead()){
    		Eth::socketStat_t sockStat = eth.socketStatus();
    		if((sockStat == Eth::SOCK_CLOSED) || (sockStat == Eth::SOCK_UDP)){
    			eth.HTTPheartbeat(localPort, serverPort, SERVER_HEARTBEAT_PATH, DEVICE);
    			eth.HTTPheartBeating(true);
    		}else{
        		eth.socketRequestStatus();
    		}
    	}

    	if(eth.HTTPheartbeatFinished() && !heartbeatTimer.isRunning()){
    		eth.HTTPheartBeating(false);
    		heartbeatTimer.startTimer();
    	}

*/    	//////////////////////////////////////////////////////


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

    		String DATA(DATA_BUFF, 145);
    		DATA += "Hola desde clon LPC845 0, en LIFEDE, via Ethernet. DNS RTX: ";
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
