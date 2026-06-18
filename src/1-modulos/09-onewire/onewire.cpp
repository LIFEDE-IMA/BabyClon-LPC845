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

OneWire::OneWire(){
	m_bit = false;
	m_presence = false;
	m_multiDataRdy = false;
	m_ROMsRdy = false;
	m_byte = 0;
	m_bitIndex = 0;
	m_slvsNumber = 0;
	m_startTime = 0;

	OneWire::init();
}

void OneWire::init(){	//	SYSCON (Cap. 8), IOCON (Cap. 11), GPIO (Cap. 12)
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 6);	//	Enable GPIO0 clk

	IOCON->PIO[16] |= (1 << 10);	// Open Drain
}

void OneWire::usDelay(uint32_t t){
	m_wantedTime = t;
	m_startTime = CTIMER0->TC;

	while((CTIMER0->TC - m_startTime) < m_wantedTime);
}

bool OneWire::readBus(){
	return ((GPIO->PIN[0] >> 6) & 1);	//	PIO0_06 STATE
}

void OneWire::setBus_low(){
	GPIO->DIRSET[0] |= (1 << 6);	//	Bus one_wire: Set Direction (PIO0_06)
	GPIO->CLR[0] |= (1 << 6);		//	Bus one-wire: OFF (PIO0_06)
}

void OneWire::releaseBus(){
	GPIO->DIRCLR[0] |= (1 << 6);	//	Bus one-wire: Clear Direction (PIO0_06), la línea queda high
}

void OneWire::startBitReading(){
	OneWire::setBus_low();	//	Write a 0 in the pin
	OneWire::usDelay(2);	//	Set 2 counts match, MR0 = TC Interrupt

	OneWire::releaseBus();
	OneWire::usDelay(10);

	m_bit = OneWire::readBus();

	OneWire::usDelay(48);
}

void OneWire::startBitWriting(bool bit){
	m_bit = bit;

	OneWire::setBus_low();	//	Write a 0 in the pin

	if(bit){	//	Writes "1"
		OneWire::usDelay(6);	//	Set 6 counts match, MR0 = TC Interrupt
		OneWire::releaseBus();
		OneWire::usDelay(64);
	}else{		//	Writes "0"
		OneWire::usDelay(60);	//	Set 60 counts match, MR0 = TC Interrupt
		OneWire::releaseBus();
		OneWire::usDelay(10);
	}
}

void OneWire::startBusReset(){
	OneWire::setBus_low();	//	Write a 0 in the pin
	OneWire::usDelay(480);	//	Set 480 counts match, MR0 = TC Interrupt

	OneWire::releaseBus();
	OneWire::usDelay(20);

	m_presence = !OneWire::readBus();
	OneWire::usDelay(410);
}

void OneWire::startByteReading(){
	m_byte = 0;

	for(m_bitIndex = 0; m_bitIndex < 8; m_bitIndex++){
		OneWire::startBitReading();	//	Starts with the first bit
		if(m_bit){
			m_byte |= (1 << m_bitIndex);
		}
	}
}

void OneWire::startByteWriting(uint8_t byte){
	m_byte = byte;

	for(m_bitIndex = 0; m_bitIndex < 8; m_bitIndex++){
		OneWire::startBitWriting(m_byte & 1);	//	Starts with the first bit
		m_byte>>=1;
	}
}

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

bool OneWire::areAllROMsRdy() const{ return m_ROMsRdy; }

uint8_t OneWire::getSlvsNumber() const{ return m_slvsNumber; }

uint8_t OneWire::getByte() const{ return m_byte; }

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

