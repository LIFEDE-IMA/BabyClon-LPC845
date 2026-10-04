/*
 * onewire.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 *  This code was written to handle onewire bus for DS18B20 temp sensor
 *  Currently, it does NOT work by interrupts, only by polling (10 Jun 2026)
 */

#ifndef ONEWIRE_H_
#define ONEWIRE_H_

#include "sctimer.h"
#include "gpio.h"

class OneWire{
	public:
		static const uint8_t MAX_BUS_SLAVES = 5;	//	Defines the maximum number of slaves per bus

		enum onewireROMcommands_t : uint8_t{
			CMD_ROM_NONE = 0,
			CMD_READ_ROM = 0x33,			//	Reads only one slave's 64-bit ROM
			CMD_MATCH_ROM = 0x55,			//	Address a specific slave device on the bus
			CMD_RESUME_ROM = 0xA5,			//	Continues comm with the last slave device addressed, saving time by not having to repeat its ROM dir
			CMD_SKIP_ROM = 0xCC,			//	Address all devices on the bus simultaneously
			CMD_ALARM_SEARCH_ROM = 0xEC,	//	Identifies the slave device that is in alarm condition
			CMD_SEARCH_ROM = 0xF0			//	Identifies all slave devices on the bus
		};

		enum globalOpState_t : uint8_t{
			OP_IDLE = 0,
			OP_BUSY,
			OP_DONE,
			OP_NO_PRESENCE,
			OP_ERROR	//	Either No Slave Answered SEARCH_ROM, Or Bad ROM CRC
		};

	private:
		//	STATES
		enum onewireState_t : uint8_t{
			OW_IDLE = 0,
			OW_RST,
			OW_WRITE_BIT,
			OW_READ_BIT,
			OW_WRITE_BYTE,
			OW_READ_BYTE,
			OW_SEARCH_ROM
		};

		onewireState_t m_onewireState;
		globalOpState_t m_globalOpState;

		//	TIMER
		SCTimer m_sctimer;

		uint32_t m_writeZeroTicks;		//	Saves the Ticks Number Needed to Keep Bus Low to Write "0" (~60us)
		uint32_t m_writeOneTicks;		//	Saves the Ticks Number Needed to Keep Bus Low to Write "1" (~6us)
		uint32_t m_writeTotalSlotTicks;	//	Saves the Ticks Number Needed to Keep Uniform Write Slot (~70us)
		uint32_t m_readStartTicks;		//	Saves the Ticks Number Needed to Keep Bus Low to Read a Nit (~3us)
		uint32_t m_readSampleTicks;		//	Saves the Ticks Number Needed to Wait Before Reading Bus Level (~12us)
		uint32_t m_readTotalSlotTicks;	//	Saves the Ticks Number Needed to Keep Uniform Read Slot (~60us)

		//	BASIC
		bool m_port;
		uint8_t m_pin;
		volatile bool m_presenceFlag;					//	Presence = true, if slave sets line LOW
		uint8_t m_slvsNumber;							//	Amount of slaves in the bus
		uint8_t m_allROMs[OneWire::MAX_BUS_SLAVES][8];	//	Each slave ROM
		volatile bool m_busBusyFlag;
		uint8_t m_byte;									//	Read / Write Byte Op
		uint8_t m_byteIndex;							//	Saves Which Bit Has to be Managed Next (Read / Write Byte Op)

		//	SEARCH ROM
		enum searchROMstate_t : uint8_t{
			SRS_RESET = 0,
			SRS_CMD,
			SRS_ID,
			SRS_CMP,
			SRS_DIR
		};

		searchROMstate_t m_searchROMstate;

		volatile bool m_searchROMactiveFlag;
		volatile bool m_ROMsRdyFlag;		//	SEARCH ROM flag
		volatile bool m_bit;
		uint8_t m_lastDiscrepancyBit;
		uint8_t m_currentDiscrepancyBit;
		uint8_t m_bitNumber;
		volatile bool m_idBit;
		volatile bool m_ctoBit;
		volatile bool m_searchDirection;
		volatile bool m_lastDevice;			//	SEARCH ROM flag
		uint8_t m_rom[8];					//	Current rom being builded in search rom

		//	BUFFER
		const uint8_t *m_txBuff;
		uint8_t m_txLen;
		uint8_t *m_rxBuff;
		uint8_t m_rxLen;
		volatile bool m_transferActiveFlag;
		uint16_t m_transferIdx;
		uint8_t m_opBuff[9];	//	Contains cmdROM + ROM
		uint8_t m_opBuffLen;
		onewireROMcommands_t m_currentROMcmd;

		void clearEventResidue(SCTimer::sctCounter_t counter, SCTimer::sctEvent_t event, SCTimer::sctRegisterNumber_t reg = SCTimer::sctREGISTER_1, SCTimer::outputNumber_t output = SCTimer::sctOUTPUT_0);	//	Clears Any Limit / Stop Set Before

		void armBitWrite(void);			//	Prepares SCTimer to Write a Bit
		void fireBitWrite(bool bit);	//	Starts SCTimer Op to Write a Bit
		void armBitRead(void);			//	Prepares SCTimer to Read a Bit
		void fireBitRead(void);			//	Starts SCTimer Op to Read a Bit

		void startBusReset(void);				//	Starts bus reset (return false if bus is busy)
		void startBitWriting(bool bit);			//	Starts one bit writing (return false if bus is busy)
		void startBitReading(void);				//	Starts one bit reading (return false if bus is busy)
		void startByteWriting(uint8_t byte);	//	Starts one byte writing (return false if bus is busy)
		void startByteReading(void);			//	Starts one byte reading (return false if bus is busy)

		void transactionHandler(void);	//	Handles OneWire Read / Write Byte
		void searchROMhandler(void);	//	Handles OneWire Search ROM

		void opDone(void);	//	Handles Next Step After Any Global Op Ends

		static void isrCallback(void);	//	Callback for SCT
		void isrHandler(void);			//	IRQ Handler

	public:
		OneWire(bool port, uint8_t pin);		//	Constructor

		bool resetBus(void);			//	Usr Reset Bus
		bool writeBit(bool bit);		//	Usr Write Bit
		bool readBit(void);				//	Usr Read Bit
		bool writeByte(uint8_t byte);	//	Usr Write Byte
		bool readByte(void);			//	Usr Read Byte

		bool startTransaction(onewireROMcommands_t cmd, const uint8_t *rom, const uint8_t *txBuff, uint8_t txLen, uint8_t *rxBuff, uint8_t rxLen);
		bool readROM(uint8_t *rom);	//	Reads ROM When Single Slave on the Bus
		void finishOp(globalOpState_t opState);

		bool isBusy(void) const;	//	Returns True if State != IDLE
		bool isPresent(void) const;	//	Returns True if Presence Pulse is Detected
		globalOpState_t getStatus(void);	//	Returns [m_globalOpState] and Sets it IDLE

		static uint8_t getCRC8(const uint8_t *data, uint8_t len);	//	Gets crc byte
		static bool isCRC8ok(const uint8_t *data, uint8_t len);		//	Returns True if CRC is OK

		bool searchROM(void);						//	Searches all salve IDs connected to the bus
		bool areAllROMsRdy(void) const;				//	Returns true if all ROMs are ready
		const uint8_t* getROM(uint8_t index) const;	//	Returns nullptr if index >= getSlvsNumber()
		uint8_t getSlvsNumber(void) const;			//	Returns the number of slaves connected

		~OneWire();									//	Destructor
};

#endif /* ONEWIRE_H_ */
