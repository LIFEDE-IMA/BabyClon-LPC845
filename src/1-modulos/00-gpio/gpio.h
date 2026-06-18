#ifndef GPIO_H_
#define GPIO_H_

#include "LPC845.h"

class Gpio{
	public:
		enum dir_t{ D_INPUT = 0, D_OUTPUT };
		enum inMode_t{ IM_NONE = 0, IM_PULLDOWN, IM_PULLUP, IM_REPEATER };
		enum outMode_t{ OM_NONE = 0, OM_OPENDRAIN };
		enum activeMode_t{ AM_HIGH = 0, AM_LOW };
		enum error_t{ E_NONE = 0};

	private:
		bool m_port;
		uint8_t m_pin;
		dir_t m_dir;
		activeMode_t m_active;
		uint8_t m_mode;		//	GPIO cant be output and input at the same time, so we have only one mode
		error_t m_error;

	public:
		static const uint8_t iocon_index[2][32];	//	IOCON-"fixed" access by port and pin

		Gpio(bool port, uint8_t pin, dir_t dir, activeMode_t active = activeMode_t::AM_HIGH);	//	Constructor
		Gpio(const Gpio &gpio);		//	Copy constructor

		void setModeIN(inMode_t inMode);	//	Sets input mode (none, pull-down, pull-up or repeater)
		void setModeOUT(outMode_t outMode);	//	Sets output mode (none, open-drain)

		void togglePin();			//	Toggles bit in port [m_port], pin [m_pin]
		void setPin();				//	Turns ON pin [m_pin] (sets the pin at 3V3)
		void clrPin();				//	Turns OFF pin [m_pin] (sets the pin at 0V)
		bool getPin() const;		//	Returns port [m_port], pin [m_pin] value
		bool getPinState() const;	//	Return port [m_port], pin [m_pin] state (depending on [m_active] value)
		uint8_t getError() const;	//	Returns current error

		~Gpio();	//	Destructor

};

#endif /* GPIO_H_ */
