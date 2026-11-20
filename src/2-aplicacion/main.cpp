#include "app.h"

int main(void){

	HW_init();	//	PLL And Systick
	app_init();	//	Current App

/*
	Dma::init();

	DmaSctTest dmaSctTest(16);
	dmaSctTest.init();

	bool f_hwReported = false;
	volatile uint8_t sctPassMask = 0;
	volatile uint8_t sctFailMask = 0;
	volatile uint8_t sctFirstFailId = 0;

	volatile uint32_t sctWords = 0;
	volatile uint32_t sctDrq = 0;
	volatile uint32_t sctEvents = 0;

	DmaPintTest dmaPintTest(0, 12, Pint::PINTSEL4, 15);
	dmaPintTest.init();

	volatile uint8_t pintPassMask = 0;
	volatile uint8_t pintFailMask = 0;
	volatile uint8_t pintFirstFailId = 0;

	volatile uint32_t pintEvents = 0;
	volatile uint32_t pintWords = 0;
*/
    while(1){
    	btMDE();	//	HC-05 BT MODULE
    	owMDE();	//	ONE-WIRE TEMP SENS ( MY18B20 )
    	mlxMDE();	//	I2C TEMP SENSOR ( MLX90614 GY-906 )
    	ethMDE();	//	ETHERNET MODULE ( W5500 )
    	wifiMDE();	//	WIFI MODULE ( ESP32 )

    	serialDisplay();	//	7 SEGMENTS SERIAL DISPLAY

/********************************************************
 *														*
 * 						DMA TEST						*
 * 														*
 ********************************************************/
/*
    	dmaSctTest.stateMachine();

    	dmaPintTest.stateMachine();

    	if(dmaSctTest.hasFinished() && dmaPintTest.hasFinished() && !f_hwReported){
    		f_hwReported = true;
    		sctPassMask = dmaSctTest.getPassMask();
    		sctFailMask = dmaSctTest.getFailMask();
    		sctFirstFailId = dmaSctTest.getFirstFailId();
    		pintPassMask = dmaPintTest.getPassMask();
    		pintFailMask = dmaPintTest.getFailMask();
    		pintFirstFailId = dmaPintTest.getFirstFailId();

    		if(dmaSctTest.allPassed() && dmaPintTest.allPassed())
    			uLedG.on();
    		else
    			uLedR.on();
    	}

    	pintEvents = dmaPintTest.getEvents();
    	pintWords = dmaPintTest.getWordsWritten();
    	sctWords = dmaSctTest.getWordsAtFirstEvent();
    	sctEvents = dmaSctTest.getEventsAtDone();
    	sctDrq = dmaSctTest.getDrqAtFirstEvent();

    	if(pintEvents >= 16){
    		volatile uint32_t trig = DMA->CHANNEL[15].CTLSTAT;
    		volatile uint32_t active = DMA->ACTIVE0;
    		volatile uint32_t busy = DMA->BUSY0;
    		volatile uint32_t ist = PINT->IST;
    		volatile uint32_t rise = PINT->RISE;
    		volatile uint32_t fall = PINT->FALL;
    		uint32_t i = ((trig - active) / (busy - ist)) + (rise / fall);
    	}
*/
    }
    return 0 ;
}
