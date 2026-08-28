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

void uploadData(void);
void heartbeat(void);

static bool f_uploadTimerExpired;
static bool f_heartbeatTimerExpired;

int main(void) {

	HW_init();
//	while(1);
	Spi spiMaster(0, 23, 0, 26, 0, 21, Spi::SPI_NUMBER_0, 1000000);
	Eth eth(0, 22, spiMaster);

	SysTimer uploadDataTimer(10, SysTimer::SINGLE, SysTimer::T_SEG, uploadData);
	SysTimer heartbeatTimer(5, SysTimer::SINGLE, SysTimer::T_SEG, heartbeat);

	uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33};

	char SERVER[] = "cifedegss.mooo.com";
	char SERVER_PATH[] = "/lab_server/guardar.php";
	char SERVER_DATA_PATH[] = "/$clonLPC_pruebaEth";
	char DEVICE[] = "monitoringLPC";
	char DATA[] = "Hola desde LPC845 via Ethernet";
	char SERVER_HEARTBEAT_PATH[] = "/lab_server/heartbeat.php";

	uint16_t localPort = 5000;
	uint16_t serverPort = 8890;

	bool f_solvingDNS = false;

	f_uploadTimerExpired = false;
	f_heartbeatTimerExpired = false;
	eth.HTTPuploading(false);
	eth.HTTPheartBeating(false);

    for(volatile int i = 0; i < 500000; i++);

//	NO DHCP	//eth.init(ip, gateway, subnet, mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);
    eth.init(mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB, Eth::MANUAL_CLOSE);

    uploadDataTimer.startTimer();
    heartbeatTimer.startTimer();

    while(1){
    	eth.handler();

    	if(eth.isReady() && !f_solvingDNS){
    		eth.DNSresolve(SERVER);
    		f_solvingDNS = true;
    	}

    	if(!eth.HTTPisBusy() && f_heartbeatTimerExpired){
    		eth.HTTPheartbeat(localPort, serverPort, SERVER_HEARTBEAT_PATH, DEVICE);
    		f_heartbeatTimerExpired = false;
    		eth.HTTPheartBeating(true);
    		heartbeatTimer.stopTimer();
    	}

    	if(eth.HTTPheartbeatFinished() && !heartbeatTimer.isRunning()){
    		eth.HTTPheartBeating(false);
    		heartbeatTimer.startTimer();
    	}

    	if(!eth.HTTPisBusy() && f_uploadTimerExpired){
    		eth.HTTPuploadData(localPort, serverPort, SERVER_PATH, SERVER_DATA_PATH, DEVICE, DATA);
    		f_uploadTimerExpired = false;
    		eth.HTTPuploading(true);
    		uploadDataTimer.stopTimer();
    	}

    	if(eth.HTTPdataUploaded() && !uploadDataTimer.isRunning()){
    		eth.HTTPuploading(false);
    		uploadDataTimer.startTimer();
    	}

    	if(eth.HTTPerrorOccurred()){
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
