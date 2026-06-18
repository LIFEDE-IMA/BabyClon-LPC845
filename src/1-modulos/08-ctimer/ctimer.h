/*
 * ctimer.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 */

#ifndef CTIMER_H_
#define CTIMER_H_

#include "LPC845.h"

#if defined (__cplusplus)
extern "C" {

void CTIMER0_IRQHandler(void);

}
#endif

class Ctimer{
	public:
		enum initType_t{
			NORMAL_CLK = 0,
			FREE_CLK,
			CAPTURE_CLK,
			HYBRID_CLK
		};

	private:
		initType_t m_clkType;

		volatile uint32_t m_ms;
		volatile uint32_t m_us;
		volatile uint32_t m_startTime;

		void init();				//	Initializes CTIMER as normal clk
		void initFreeClk();			//	Initializes CTIMER as free clk
		void initCaptureClk();		//	Initializes CTIMER as capture clk
		void initHybrid();			//	Initializes CTIMER as capture & free clk

		void isrHandler();			//	Normal CTIMER ISR Handler
		void freeClk_isrHandler();	//	Free clk CTIMER ISR Handler
		void captClk_isrHandler();	//	Capture clk CTIMER ISR Handler
		void hybClk_isrHandler();	//	Hybrid clk CTIMER ISR Handler

	public:
		static uint8_t ctimerSources;

		Ctimer(initType_t clkType);					//	Constructor

		void set_usTimer(uint32_t us);
		void stop_usTimer();

		friend void CTIMER0_IRQHandler(void);	//	ISR Handler

		~Ctimer();					//	Destructor
};


#endif /* CTIMER_H_ */
