/*
 * pint.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#ifndef PINT_H_
#define PINT_H_

#include "LPC845.h"
#include "inmux.h"

#if defined (__cplusplus)
extern "C" {

void PININT0_IRQHandler(void);
void PININT1_IRQHandler(void);
void PININT2_IRQHandler(void);
void PININT3_IRQHandler(void);
void PININT4_IRQHandler(void);
void PININT5_IRQHandler(void);
void PININT6_IRQHandler(void);
void PININT7_IRQHandler(void);

}
#endif

class Pint{
	public:
		static const uint8_t TOTAL_PINT_SOURCES = 8;

		enum pintSource_t : uint8_t{
			PINTSEL0 = 0,
			PINTSEL1,
			PINTSEL2,
			PINTSEL3,
			PINTSEL4,
			PINTSEL5,
			PINTSEL6,
			PINTSEL7,
			ANY
		};

		enum triggerType_t{ T_FALLINGEDGE = 0, T_RISINGEDGE, T_BOTHEDGES, T_LOWLVL, T_HIGHLVL, T_NONE };

	private:
		pintSource_t m_pintSource;
		static uint8_t m_usedSources;

		bool m_port;
		uint8_t m_pin;
		triggerType_t m_trigger;

		void (*m_handler)(void);

		void configIntExt();	//	Configures external interrupts

	public:
		Pint(bool port, uint8_t pin, triggerType_t trigger, void (*handler)(void), pintSource_t pintSource = pintSource_t::ANY);		//	Constructor

		pintSource_t getSource(void) const;	//	Returns source for current pin interrupt [0 : 7]

		bool getDmaTrigger(InMux::DMA_TRIGGER_INPUT_SOURCE_t &trigger) const;	//	Returns True and Modifies [trigger] if Current Source Has Valid DMA TRIGGER INPUT (PININT4 to PININT7)

		friend void PININT0_IRQHandler(void);	//	ISR Handler for PININT0
		friend void PININT1_IRQHandler(void);	//	ISR Handler for PININT1
		friend void PININT2_IRQHandler(void);	//	ISR Handler for PININT2
		friend void PININT3_IRQHandler(void);	//	ISR Handler for PININT3
		friend void PININT4_IRQHandler(void);	//	ISR Handler for PININT4
		friend void PININT5_IRQHandler(void);	//	ISR Handler for PININT5
		friend void PININT6_IRQHandler(void);	//	ISR Handler for PININT6
		friend void PININT7_IRQHandler(void);	//	ISR Handler for PININT7

		~Pint();	//	Destructor
};

#endif /* PINT_H_ */
