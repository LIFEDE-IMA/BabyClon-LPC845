/*
 * onewire.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 *  Currently, it does NOT work by interrupts, only by polling (10 Jun 2026)
 */

#include "onewire.h"

volatile bool f_busRestFinished = false;
volatile bool f_cmdSent = false;
volatile bool f_byteRead = false;

OneWire *OneWireInstance = nullptr;

OneWire::OneWire(bool port, uint8_t pin) : m_sctimer(SCTimer::sctUNIFIED_MODE, &OneWire::isrCallback){
	//	OneWire Config
	OneWireInstance = this;
	m_port = port;
	m_pin = pin;
	m_bit = 0;
	m_presenceFlag = false;
	m_byte = 0;
	m_byteIndex = 0;
	m_busBusyFlag = false;
	m_ROMsRdyFlag = false;
	m_slvsNumber = 0;
	m_onewireState = onewireState_t::OW_IDLE;

	//	IOCON OPEN-DRAIN
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 18);	//	Enable IOCON clk
	IOCON->PIO[Gpio::iocon_index[m_port][m_pin]] |= (Gpio::OM_OPENDRAIN << 10);
	SYSCON->SYSAHBCLKCTRL0 &= ~(1 << 18);	//	Disable IOCON clk (saves power)

	//	SCTimer Config
	m_sctimer.configInput(InMux::SCT_INPUT_NUMBER_t::INPUT_0, InMux::SCT_INMUX_SOURCE_t::SCT_PIN0, m_port, m_pin);
	m_sctimer.configOutput(SCTimer::sctOUTPUT_0, SCTimer::outputType_t::sctOUTPUTtype_PIN, m_port, m_pin);
	m_sctimer.configRegisterMode(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_0, SCTimer::regMATCH_MODE);		//	Timing
	m_sctimer.configRegisterMode(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_1, SCTimer::regCAPTURE_MODE);	//	Reset / presence / reading
	m_sctimer.configRegisterMode(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_3, SCTimer::regMATCH_MODE);		//	Reading Timing

	m_sctimer.init();
	m_sctimer.setOutput(SCTimer::sctOUTPUT_0);	//	Sets Output Pin High

	//	This Saves CPU Time During OW Protocol
	m_writeZeroTicks = m_sctimer.baseToTicks(SCTimer::UNIFIED_COUNTER, 57, SCTimer::T_MICRO);
	m_writeOneTicks = m_sctimer.baseToTicks(SCTimer::UNIFIED_COUNTER, 3, SCTimer::T_MICRO);
	m_writeTotalSlotTicks = m_sctimer.baseToTicks(SCTimer::UNIFIED_COUNTER, 60, SCTimer::T_MICRO);
	m_readStartTicks = m_sctimer.baseToTicks(SCTimer::UNIFIED_COUNTER, 1, SCTimer::T_MICRO);
	m_readSampleTicks = m_sctimer.baseToTicks(SCTimer::UNIFIED_COUNTER, 9, SCTimer::T_MICRO);
	m_readTotalSlotTicks = m_sctimer.baseToTicks(SCTimer::UNIFIED_COUNTER, 50, SCTimer::T_MICRO);


	m_sctimer.enableNVICint();
}

void OneWire::clearEventResidue(SCTimer::sctCounter_t counter, SCTimer::sctEvent_t event, SCTimer::sctRegisterNumber_t reg, SCTimer::outputNumber_t output){
	m_sctimer.clearLimit(counter, event);
	m_sctimer.clrStop(counter, event);
	m_sctimer.clearCaptureTrigger(counter, reg, event);
	m_sctimer.clearEventOutputSet(output, event);
	m_sctimer.clearEventOutputClear(output, event);
	m_sctimer.disableEventInterrupt(event);
	m_sctimer.clearEventIntFlag(event);
}

bool OneWire::resetBus(void){
	if(m_busBusyFlag)	return false;

	m_busBusyFlag = true;
	m_onewireState = onewireState_t::OW_RST;
	m_presenceFlag = false;

	m_sctimer.haltCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.setState(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_STATE_0);

	//	CLEAR ANY PREVIOUS LIMIT / STOP FOR REQUIRED EVENTS
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0);
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1);
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);

	// STATE 0: Set Bus Low ~480us -> Release -> State 1
	m_sctimer.setTimer(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_0, 480, SCTimer::T_MICRO);
	m_sctimer.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0, SCTimer::sctREGISTER_0,
						  SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT, SCTimer::STATE_LOAD, SCTimer::sctEVENT_STATE_1);	//	When count == 480us, STATE = 1 (State 0 -> State 1)
	m_sctimer.configEventOutputSet(SCTimer::sctOUTPUT_0, SCTimer::sctEVENT_0);	//	Event 0 Sets Output 0 Pin Low (Bus Low)
	m_sctimer.setLimit(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0);	//	Reset Count to "0" Entering State 1
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_0, SCTimer::sctEVENT_STATE_0);	//	If State != 0, Event 0 Is Not Enabled

	//	STATE 1: Capture Bus Falling Edge (Presence) -> Close Window at 300us -> Stop
	m_sctimer.configEventInput(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1,
							   SCTimer::sctREGISTER_0, InMux::SCT_INPUT_NUMBER_t::INPUT_0,
							   SCTimer::FALLING_EDGE, SCTimer::COMBMODE_IO, SCTimer::EQUAL,
							   SCTimer::DIR_INDEPENDENT);	//	Match Register Sel is Irrelevant Here (Combmode = IO)
	m_sctimer.setCaptureTrigger(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_1, SCTimer::sctEVENT_1);	//	Saves Counter Count In Register 1 When Event 1 Fires
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_1, SCTimer::sctEVENT_STATE_1);	//	If State != 1, Event 1 Is Not Enabled

	m_sctimer.setTimer(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_2, 360, SCTimer::T_MICRO);
	m_sctimer.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2, SCTimer::sctREGISTER_2,
						  SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT, SCTimer::STATE_LOAD, SCTimer::sctEVENT_STATE_0);	//	Event 2 Fires When Count == 300us, Load Event 0 (Reset State Machine)
	m_sctimer.setLimit(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);	//	Reset Count to "0" When Event 2 Fires
	m_sctimer.setStop(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);	//	After Second Timer Hits Limit (300 us), Stop Counter
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_2, SCTimer::sctEVENT_STATE_1);	//	If State != 1, Event 2 Is Not Enabled
	m_sctimer.enableEventInterrupt(SCTimer::sctEVENT_2);	//	Just the Last Event Requieres CPU Handling

	m_sctimer.clrOutput(SCTimer::sctOUTPUT_0);	//	Set Bus Low
	m_sctimer.startCounter(SCTimer::UNIFIED_COUNTER);

	return true;
}

void OneWire::armBitWrite(void){
	//	Only State 0 Required
	m_sctimer.haltCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.setState(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_STATE_0);

	//	CLEAR ANY PREVIOUS LIMIT / STOP FOR REQUIRED EVENTS
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0);
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1);
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);

	//	EVENT 0: Set Bus Low ~6us (if bit == 1) or ~60us (if bit == 0) -> Decided In fireBitWrite()
	m_sctimer.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0, SCTimer::sctREGISTER_0,
						  SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT);	//	When count == [us]us, STATE = 0 (State 0 -> State 0, No Change)
	m_sctimer.configEventOutputSet(SCTimer::sctOUTPUT_0, SCTimer::sctEVENT_0);	//	Event 0 Sets Output 0 Pin Low (Bus Low)
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_0, SCTimer::sctEVENT_STATE_0);	//	If State != 0, Event 0 Is Not Enabled

	//	EVENT 1: Whole Time Window (bit independent) ~70us
	m_sctimer.setMatch(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_2, m_writeTotalSlotTicks);
	m_sctimer.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1, SCTimer::sctREGISTER_2,
						  SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT);	//	When count == 70us, STATE = 0 (State 0 -> State 0, No Change)
	m_sctimer.setLimit(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1);	//	Reset Count to "0" When Event 1 Fires
	m_sctimer.setStop(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1);	//	After Second Timer Hits Limit (70 us), Stop Counter
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_1, SCTimer::sctEVENT_STATE_0);	//	If State != 0, Event 1 Is Not Enabled
	m_sctimer.enableEventInterrupt(SCTimer::sctEVENT_1);	//	Just the Last Event Requieres CPU Handling
}

void OneWire::fireBitWrite(bool bit){
	uint32_t ticks = 0;
	ticks = (bit ? m_writeOneTicks : m_writeZeroTicks);

	m_sctimer.haltCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.clearCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.setMatch(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_0, ticks);

	m_sctimer.clrOutput(SCTimer::sctOUTPUT_0);	//	Set Bus Low
	m_sctimer.startCounter(SCTimer::UNIFIED_COUNTER);
}

void OneWire::armBitRead(void){
	//	Only State 0 Required
	m_sctimer.haltCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.setState(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_STATE_0);

	//	CLEAR ANY PREVIOUS LIMIT / STOP FOR REQUIRED EVENTS
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0);
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1);
	clearEventResidue(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);

	//	EVENT 0: Set Bus Low ~3us -> Release Bus
	m_sctimer.setMatch(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_0, m_readStartTicks);
	m_sctimer.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_0, SCTimer::sctREGISTER_0,
						  SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT);	//	When count == [us]us, STATE = 0 (State 0 -> State 0, No Change)
	m_sctimer.configEventOutputSet(SCTimer::sctOUTPUT_0, SCTimer::sctEVENT_0);	//	Event 0 Sets Output 0 Pin Low (Bus Low)
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_0, SCTimer::sctEVENT_STATE_0);	//	If State != 0, Event 0 Is Not Enabled

	//	EVENT 1: Wait ~12us AND if Input == Low Level -> Set EVFLAG ( COMBMODE_AND )
	m_sctimer.setMatch(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_3, m_readSampleTicks);
	m_sctimer.configEventInput(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_1,
							   SCTimer::sctREGISTER_3, InMux::SCT_INPUT_NUMBER_t::INPUT_0,
							   SCTimer::LOW_LEVEL, SCTimer::COMBMODE_AND, SCTimer::EQUAL,
							   SCTimer::DIR_INDEPENDENT);	//	Match Register Sel is Relevant Here (Combmode = AND)
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_1, SCTimer::sctEVENT_STATE_0);	//	If State != 0, Event 1 Is Not Enabled

	//	EVENT 2: Whole Time Window ~60us -> Interrupt
	m_sctimer.setMatch(SCTimer::UNIFIED_COUNTER, SCTimer::sctREGISTER_2, m_readTotalSlotTicks);
	m_sctimer.configEvent(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2, SCTimer::sctREGISTER_2,
						  SCTimer::EQUAL, SCTimer::DIR_INDEPENDENT);	//	When count == 60us, STATE = 0 (State 0 -> State 0, No Change)
	m_sctimer.setLimit(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);	//	Reset Count to "0" When Event 2 Fires
	m_sctimer.setStop(SCTimer::UNIFIED_COUNTER, SCTimer::sctEVENT_2);	//	After Third Timer Hits Limit (60 us), Stop Counter
	m_sctimer.enableStateEvent(SCTimer::sctEVENT_2, SCTimer::sctEVENT_STATE_0);	//	If State != 0, Event 2 Is Not Enabled
	m_sctimer.enableEventInterrupt(SCTimer::sctEVENT_2);	//	Just the Last Event Requieres CPU Handling
}

void OneWire::fireBitRead(void){
	m_sctimer.haltCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.clearCounter(SCTimer::UNIFIED_COUNTER);
	m_sctimer.clrOutput(SCTimer::sctOUTPUT_0);	//	Set Bus Low
	m_sctimer.startCounter(SCTimer::UNIFIED_COUNTER);
}

bool OneWire::writeBit(bool bit){
	if(m_busBusyFlag)	return false;

	m_busBusyFlag = true;
	m_onewireState = onewireState_t::OW_WRITE_BIT;

	OneWire::armBitWrite();		//	Configures SCTimer to Handle Write Op
	OneWire::fireBitWrite(bit);	//	Sarts SCTimer in Bit Write Mode

	return true;
}

bool OneWire::readBit(void){
	if(m_busBusyFlag)	return false;

	m_busBusyFlag = true;
	m_onewireState = onewireState_t::OW_READ_BIT;
	OneWire::armBitRead();		//	Configures SCTimer to Handle Read Op
	OneWire::fireBitRead();		//	Sarts SCTimer in Bit Read Mode

	return true;
}

bool OneWire::writeByte(uint8_t byte){
	if(m_busBusyFlag)	return false;

	f_cmdSent = false;
	m_busBusyFlag = true;
	m_onewireState = onewireState_t::OW_WRITE_BYTE;
	m_byte = byte;
	m_byteIndex = 0;

	OneWire::armBitWrite();	//	Configures SCTimer to Handle Write Op
	OneWire::fireBitWrite(((m_byte >> m_byteIndex) & 0x1));	//	Starts with the first bit (LSB First)

	return true;
}



bool OneWire::readByte(void){
	if(m_busBusyFlag)	return false;

	f_byteRead = false;
	m_busBusyFlag = true;
	m_onewireState = onewireState_t::OW_READ_BYTE;
	m_byte = 0;
	m_byteIndex = 0;

	OneWire::armBitRead();	//	Configures SCTimer to Handle Read Op
	OneWire::fireBitRead();	//	Sarts SCTimer in Bit Read Mode

	return true;
}

/*
uint8_t OneWire::getCRC(uint8_t *scratchpad, uint8_t len){
	int i, j;
	uint8_t crc = 0;

	for(i = 0; i < len; i++){
		uint8_t aux = scratchpad[i];	//	Isolates each byte
		for(j = 0; j < 8; j++){	//	Isolates each bit
			uint8_t mix = ((crc ^ aux) & 0x01);
			crc >>= 1;

			if(mix){
				crc ^= 0x8C;
			}
			aux >>= 1;
		}
	}
	return crc;
}

void OneWire::startROMsearch(){
	int i = 0, j = 0;
	uint8_t crc = 0;
	m_slvsNumber = 0;
	for(i = 0; i < 8; i++) m_rom[i] = 0;
	for(i = 0; i < MAX_SLAVES; i++){
		for(j = 0; j < 8; j++){
			m_allROMs[i][j] = 0;
		}
	}
	m_lastDiscrepancyBit = m_searchDirection = 0;
	m_lastDevice = false;
	m_ROMsRdy = false;

	while(!m_lastDevice && (m_slvsNumber < MAX_SLAVES)){
		OneWire::startBusReset();
		OneWire::startByteWriting(CMD_SEARCH_ROM); // Identifies all slave devices on the bus
		m_bitNumber = m_bitMask = 1;
		m_currentDiscrepancyBit = m_byteNumber = 0;

		while(m_byteNumber < 8){ // ROM size
			OneWire::startBitReading(); // AND between first bit of all slaves
			m_idBit = m_bit;
			OneWire::startBitReading(); // AND between the complement of the first bit of all slaves
			m_ctoBit = m_bit;
			if(m_idBit && m_ctoBit){ // Both 1
				return; // Error
			}else if(!m_idBit && !m_ctoBit){ // Both 0 => Theres conflict
				if(m_bitNumber == m_lastDiscrepancyBit){ // We are "standing" on a bit we previously had conflict
					m_searchDirection = 1; // Follow "1" path (cause the first time we find a conflict, we follow "0" path)
				}else if(m_bitNumber > m_lastDiscrepancyBit){ // Found new conflict, further ahead than previous one
					m_searchDirection = 0; // Follow "0" path (cause its the first time we found THIS conflict)
					m_currentDiscrepancyBit = m_bitNumber;
				}else{ // Previous path to the last time we solved a conflict
					m_searchDirection = (m_rom[m_byteNumber] & m_bitMask); // Last saved bit
					if(!m_searchDirection){
						m_currentDiscrepancyBit = m_bitNumber;
					}
				}
			}else{ // Theres NO conflict
				m_searchDirection = m_idBit; // Same path for all the IDs ("0" or "1")
			}
			if(m_searchDirection){
				m_rom[m_byteNumber] |= m_bitMask; // Add "1" to the ROM being builded
			}else{
				m_rom[m_byteNumber] &= ~m_bitMask; // Add "0" to the ROM being builded
			}
			OneWire::startBitWriting(m_searchDirection); // Writes chosen bit (IDs that does NOT have their bit in this position
														//  with same value as searchDirection, are discarded in the current tree branch)
			m_bitNumber++;
			m_bitMask <<= 1;

			if(m_bitMask == 0){ // 8 bits were evaluated
				m_byteNumber++;
				m_bitMask = 1;
			}
		}
		m_lastDiscrepancyBit = m_currentDiscrepancyBit; // Next iteration goes through another tree branch (the most distant conflict of the first bit)
		if(m_lastDiscrepancyBit == 0){ // Theres NO more conflicts
			m_lastDevice = true;
		}
		for(i = 0; i < 8; i++){
			crc = OneWire::getCRC(m_rom, 7);
			if(crc != m_rom[7]){
				continue; // Invalid ROM
			}else{
				m_allROMs[m_slvsNumber][i] = m_rom[i]; // Saves found ROM
			}
		}
		m_slvsNumber++;
	}
	m_ROMsRdy = true;
}



bool OneWire::areAllROMsRdy(void) const{ return m_ROMsRdy; }
*/
uint8_t OneWire::getSlvsNumber(void) const{ return m_slvsNumber; }

uint8_t OneWire::getByte(void) const{ return m_byte; }

void OneWire::isrCallback(void){
	if(OneWireInstance)
		OneWireInstance->isrHandler();
}

void OneWire::isrHandler(void){
	uint8_t flags = m_sctimer.getIntFlags();
	m_sctimer.clearIntFlags(flags);

	switch(m_onewireState){
		case onewireState_t::OW_IDLE:
			//	Nothing To Do
			break;

		case onewireState_t::OW_RST:
			m_presenceFlag = ((flags & (1 << SCTimer::sctEVENT_1)) != 0);	//	If Event 1 Occurred, Slave Pulled-Down the Line
			m_onewireState = onewireState_t::OW_IDLE;
			f_busRestFinished = true;
			m_busBusyFlag = false;
			break;

		case onewireState_t::OW_WRITE_BIT:
			m_onewireState = onewireState_t::OW_IDLE;
			m_busBusyFlag = false;
			break;

		case onewireState_t::OW_READ_BIT:
			m_bit = !((flags & (1 << SCTimer::sctEVENT_1)) != 0);	//	COMBMODE_AND -> EVFLAG = 1 if 12us Timer Expired and Bus is Low Level
			m_onewireState = onewireState_t::OW_IDLE;
			m_busBusyFlag = false;
			break;

		case onewireState_t::OW_WRITE_BYTE:
			m_byteIndex++;
			if(m_byteIndex < 8){
				OneWire::fireBitWrite(((m_byte >> m_byteIndex) & 0x1));	//	Next Bit
			}else{
				f_cmdSent = true;
				m_onewireState = onewireState_t::OW_IDLE;
				m_busBusyFlag = false;
			}
			break;

		case onewireState_t::OW_READ_BYTE:
			m_bit = !((flags & (1 << SCTimer::sctEVENT_1)) != 0);	//	COMBMODE_AND -> EVFLAG = 1 if 12us Timer Expired and Bus is Low Level
			m_byte |= (m_bit << m_byteIndex);
			m_byteIndex++;

			if(m_byteIndex < 8){
				OneWire::fireBitRead();	//	Next Bit
			}else{
				f_byteRead = true;
				m_onewireState = onewireState_t::OW_IDLE;
				m_busBusyFlag = false;
			}
			break;

		default:
			//	ERROR
			break;
	}
}

OneWire::~OneWire(){}


//	-----------------	SEARCH ROM OPERATION	-----------------	//
//	 ( Para pensar esta parte la tuve que escribir en español xd)

//	Todos los slaves siguen la siguiente directiva:
//	Envían el primer bit de su ID seguido por el complemento (CTO) de ese bit
//	El bus funciona como un AND físico (si alguno de todos los slaves tiene un 0 en ese bit, la linea queda en LOW)
//	Si el bit es 0 => Algun slave tiro el bus a 0
//	Si el bit es 1 => Todos los slaves tienen un 1 en ese bit (ninguno tiró el bus a 0)
//	Entonces surgen 4 posibilidades:
//		Caso A (bit = 0, CTO = 1):	Todos los slaves tienen un 0 en esa posición del ID
//		Caso B (bit = 1, CTO = 0):	Todos los slaves tienen un 1 en esa posición del ID
//		Caso C (bit = 0, CTO = 0):	Conflicto (Hay slaves que tienen un 0 y slaves que tienen un 1 en esa posición del ID)
//		Caso D (bit = 1, CTO = 1):	Error de comunicación (Si todos los bits son 1, todos los complementos no pueden ser 1)

//	Ejemplo:
//		Slave 1: 1101...
//		Slave 2: 1010...							  bit cto
//		Al leer 2 bits del bus luego de CDM_READ_ROM: 1&1 0&0

//	Una vez leídos el primer bit y su cto, debe escribirse un "Bit de decisión"
//	Si decisionBit==0, entonces todos los slaves cuyo ID no tenga un 0 en esa posición que fue leída se desactivan
//	Si decisionBit==1, entonces todos los slaves cuyo ID no tenga un 1 en esa posición que fue leída se desactivan

//	Luego de escribir el bit de decisión, los slaves que siguen activos envían su siguiente bit del ID junto a su CTO
//	Se repite el proceso hasta leer 128bits (64 de ROM y 64 ctos)

//	Entonces, los casos A, B y D son de rápida solución: en A y B todos los slaves tienen el mismo bit => escribimos ese bit como decisionBit
//														 en D sabemos que tenemos un error y debemos reiniciar la comunicación
//	El caso C requiere de elegir un "camino de exploración" a seguir (0 o 1) y guardar esa posición de conflicto para elegir el camino diferente la siguiente iteración
//	Usando como ejemplo Slave 1 y Slave 2, en la segunda lectura llega un conflicto: bit=1&0, cto=0&1 => leo: 0 0
//	Por lo tanto elijo escribir 1 y eso provoca que se desactive el Slave 2 => en la siguiente lectura solo tengo un slave activo y por lo tanto cada vez que lea no voy a tener conflictos y termino encontrando su ID
//	Una vez obtenida la ID del Slave 1, vuelvo a la posición donde tuve el primer conflicto y ahora escribo un 0 => se desactiva el Slave 1 => obtengo ID del Slave 2
//	Este mismo procedimiento se realiza con X slaves, solo varía la cantidad de iteraciones que hay que efectuar

