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

int main(void) {

	HW_init();
//	while(1);
	Spi spiMaster(0, 23, 0, 26, 0, 21, Spi::SPI_NUMBER_0, 1000000);
	Eth eth(0, 22, spiMaster);

	uint8_t ip[4] = {192, 168, 0, 50};
	uint8_t gateway[4] = {192, 168, 0, 1};
	uint8_t subnet[4] = {255, 255, 255, 0};
	uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33};

	uint8_t txMsg[] = "Hola desde LPC845";
	uint8_t txMsg2[] = "Hola desde LPC845, via modulo ethernet w5500, este mensaje es bastante largo para superar los 128 bytes por transferencia que tiene el buffer. Hoy es martes 11 de Agosto y el total son 194 bytes";
	uint8_t rxMsg[250] = {0};

	bool f_openStarted = false;
	bool f_connectStarted = false;
	bool f_sendStarted = false;
	bool f_receiveStarted = false;
	bool f_closeStarted = false;
	bool f_initialCloseStarted = false;

    for(volatile int i = 0; i < 500000; i++);

    eth.init(ip, gateway, subnet, mac, Eth::SOCKBUF_2KB, Eth::SOCKBUF_2KB,Eth::MANUAL_CLOSE);


    while(1){
    	eth.handler();

    	if(!f_initialCloseStarted && !eth.socketCloseFinished()){
    		eth.socketClose();
    		f_initialCloseStarted = true;
    	}

    	if(eth.isReady() && !f_openStarted){
    		eth.socketOpenTCP(5000);
    		f_openStarted = true;
    	}

    	if(eth.socketOpened() && !f_connectStarted){
    		uint8_t serverIP[4] = {192, 168, 0, 7};

    		eth.socketConnect(serverIP, 5000);

    		f_connectStarted = true;
    	}

    	if(eth.socketConnected()){
    		if(!f_sendStarted){
    			eth.socketSend(txMsg2, (sizeof(txMsg2) - 1));
    			f_sendStarted = true;
    		}
        	if(eth.socketSendFinished() && !f_receiveStarted){
    			eth.socketReceive(rxMsg, (sizeof(rxMsg)));
    			f_receiveStarted = true;
        	}
        	if(eth.socketReceiveFinished() && eth.socketSendFinished()){
        		f_sendStarted = false;
        		f_receiveStarted = false;
        		static uint16_t i = 0;
        		i++;
        		if(i >= 1000){
        			i = 0;
        			static uint8_t j = 0;
        			j++;
        			if(j >= 100){
        				j = 0;
        				eth.socketDisconnect();
        				f_closeStarted = true;
        			}
        		}
        	}
    	}
        if(f_closeStarted && eth.socketDisconnectFinished()){
        	f_closeStarted = false;
        	f_openStarted = false;
        	f_connectStarted = false;
        	f_sendStarted = false;
        	f_receiveStarted = false;
        	for(volatile int i = 0; i < 5000000; i++);
        	//	BREAKPOINT
        }
    }
    return 0 ;
}
