/*
 * app.h
 *
 *  Created on: 31 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This header was written to handle main.cpp app
 */

#ifndef APP_H_
#define APP_H_

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
#include "my_string.h"
#include "led.h"
#include "bt.h"
#include "dma.h"

/*#######################################################################
  #							TIMER HANDLERS								#
  #######################################################################*/

void heartbeat_timerHandler(void);
void dnsRetry_timerHandler(void);
void searchROM_timerHandler(void);
void convertT_timerHandler(void);
void readOwTemp_timerHandler(void);
void readMlxTemp_timerHandler(void);


/*#######################################################################
  #								MDE LINKERS								#
  #######################################################################*/

//	DISPLAY
bool updateDisplay(void);
void setUpdateDisplay(void);
void clearUpdateDisplay(void);

//	SENSORS - BLUETOOTH
void checkForNewTemps(void);
bool allTempsRdy(void);
void newTemps(void);
void noNewTemps(void);

//	ETH - WIFI INTERFACES
bool ethMode(void);
bool wifiMode(void);
void setEthMode(void);
void setWifiMode(void);
bool dataRdyForUpload(void);
void setUploadRdyFlag(void);
void clearUploadRdyFlag(void);


/*#######################################################################
  #							  APP PROTOTIPES							#
  #######################################################################*/

void app_init(void);

void btMDE(void);
void owMDE(void);
void mlxMDE(void);
void ethMDE(void);
void wifiMDE(void);

void serialDisplay(void);


#endif /* APP_H_ */
