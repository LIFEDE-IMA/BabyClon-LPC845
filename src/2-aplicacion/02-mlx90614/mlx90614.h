/*
 * mlx90614.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 *
 * 	This class was written to handle MLX90614 temperature sensor with I2C peripheral
 */

#ifndef MLX90614_H_
#define MLX90614_H_

#include "i2c.h"
#include "systimer.h"

class MLX90614{
	public:
		static const uint8_t DEFAULT_ADDR = 0x5A; 		// Factory Sensor Address

		//	RAM (Read Only)
		enum ramAddress_t : uint8_t{
			RAM_RAW_IR1 = 0x04,	//	Raw Data IR Channel 1 Address
			RAM_RAW_IR2 = 0x05,	//	Raw Data IR Channel 2 Address
			RAM_TAMB = 0x06,	//	Ambient Temperature Address
			RAM_TOBJ = 0x07,	//	Object Temperature Address (Zone 1)
			RAM_TOBJ2 = 0x08	//	Object Temperature Address (Zone 2, Dual Zone Sensors Only)
		};

		//	EEPROM
		enum eepromAddress_t : uint8_t{
			EE_TOMAX = 0x00,		//	PWM Range Max / Thermal Relay Hysteresis
			EE_TOMIN = 0x01,		//	PWM Range Min / Thermal Relay Threshold
			EE_PWMCTRL = 0x02,		//	PWM / Thermal Relay Control
			EE_TARANGE = 0x03,		//	Ambient Range for PWM
			EE_EMISSIVITY = 0x04,	//	Emissivity Correction Coefficient
			EE_CONFIG1 = 0x05,		//	Config Register 1 (Filters, etc)
			EE_SMBUS_ADDR = 0x0E,	//	Slave Address (LSB Only)
		};

		//	CONFIG REGISTER 1 (Bits [2:0] -> Percentage = How Much of Each New Sample Enters the Output (100% = No IIR)
		enum IIR_t : uint8_t{
			P100 = 0,
			P80,
			P67,
			P57,
			P50,
			P25,
			P16,
			P13
		};

		//	CONFIG REGISTER 1 (Bits [10:8] -> Number of Samples Averaged; < 128 are NOT Recommended)
		enum FIR_t : uint8_t{
			N128 = 4,
			N256,
			N512,
			N1024
		};

		//	STATUS
		enum mlxStatus_t : uint8_t{
			IDLE = 0,
			BUSY,
			OP_DONE,	//	Last Op Finished OK
			ERR_I2C,	//	I2C transfer() Failed
			ERR_PEC,	//	Corrupted Data
			ERR_BAD_T,	//	Sensor Flagged the Temp as Invalid
			ERR_VERIFY,	//	EEPROM Read Back is Different from What was Written
			ERR_PARAM	//	Wrong Value / Not Possible With the Current EEPROM Content
		};

		//	CMD OPCODE
		static const uint8_t CMD_RAM = 0x00;		//	000x xxxx
		static const uint8_t CMD_EEPROM = 0x20;		//	001x xxxx
		static const uint8_t CMD_UNLOCK_KE = 0x60;	//	Unlocks EEPROM Cell 0x0F (Emissivity Applications)
		static const uint8_t CMD_SLEEP = 0xFF;		//	1111 1111

		static const uint8_t EEPROM_DELAY_MS = 10;	//	Erase and Write EEPROM Take 5ms Each, 10ms is the Recommended Value in Datasheet

		static const uint16_t CFG_DUAL_MASK = (1 << 6);	//	Bit 6 from Config Reg 1: "0" Single IR Sensor, "1" Dual IR Sensor
		static const uint16_t CFG_IIR_MASK = 0x07;		//	[2:0]
		static const uint16_t CFG_FIR_MASK = 0x0700;	//	[10:8]

		static const uint16_t RAM_ERROR_FLAG = 0x8000;

		static const float GAIN_TABLE[8];

	private:
		static const uint8_t MAX_READS = 4;
		static const uint8_t MAX_WRITE_RETRIES = 3;

		enum mlxState_t : uint8_t{
			MLX_IDLE,
			MLX_READ_REQUEST,	//	Starts Reading m_readCmdList[m_readIdx]
			MLX_READ_WAIT,		//	Waits for I2C, Checks PEC, Saves Data
			MLX_EE_MODIFY,		//	Computes the New EEPROM Value
			MLX_EE_ERASE_REQUEST,
			MLX_EE_ERASE_WAIT,
			MLX_EE_ERASE_DELAY,
			MLX_EE_WRITE_REQUEST,
			MLX_EE_WRITE_WAIT,
			MLX_EE_WRITE_DELAY,
			MLX_EE_RETRY_DELAY,	//	Waits Before Writing Again After a Failed Write
			MLX_EE_VERIFY,
			MLX_SLEEP_REQUEST,
			MLX_SLEEP_WAIT
		};

		I2C &m_i2cMaster;
		SysTimer m_delayTimer;	//	EEPROM Erase / Write Time

		uint8_t m_addr;
		bool m_dualFlag;		//	Dual Zone Sensor (Tobj2 Valid)
		bool m_twoChannelsFlag;	//	Sensor Processes 2 IR Channels (Dual Zone or Gradient Compensated Parts)
		bool m_sleepingFlag;
		bool m_sleepJobFlag;

		mlxState_t m_mlxState;
		mlxStatus_t m_mlxStatus;
		I2C::i2cStatus_t m_i2cStatus;

		float m_ambTemp;
		float m_obj1Temp;
		float m_obj2Temp;
		int16_t m_rawIR1;
		int16_t m_rawIR2;
		uint16_t m_emissivityRaw;
		uint16_t m_config;
		uint16_t m_eepromData;

		uint8_t m_readCmdList[MLX90614::MAX_READS];
		uint8_t m_readCount;
		uint8_t m_readIndex;
		mlxState_t m_stateAfterReading;	//	After the Last Read of the List
		uint16_t m_word;

		uint8_t m_eepromAddr;
		uint16_t m_eepromMask;
		uint16_t m_eepromValue;
		uint16_t m_eepromNewValue;
		uint8_t m_writeTries;

		uint8_t m_rxBuff[3];	//	LSB, MSB, PEC
		uint8_t m_txBuff[4];	//	CMD, LSB, MSB, PEC

		static uint8_t crc8(const uint8_t *data, uint8_t len);
		static float fourthRoot(float x, float guess);

		bool canStart(void) const;	//	False if Busy / Sleeping
		void startReading(const uint8_t *cmds, uint8_t count, mlxState_t stateAfter);
		void startEEPROMwriting(uint8_t eepromAddr, uint16_t mask, uint16_t value);	//	Read, Modify, Erase, Write and Verify one EEPROM Cell
		void armWrite(uint8_t cmd, uint16_t value);

		void startDelay(void);	//	Starts Timer

		void readFinished(void);
		bool storeRead(uint8_t cmd);

		void eepromModify(void);

		void failI2C(void);
		void finish(mlxStatus_t status);

	public:
		MLX90614(I2C &i2cMaster, uint8_t address = MLX90614::DEFAULT_ADDR, bool dualZone = false);	//	Constructor, dualZone = true for xBx Parts

		void stateMachine(void);	//	Runs PWM, Sleep, EEPROM Modify, ... , Modes

		bool readTamb(void);		//	Starts ambient temperature reading
		bool readTobj(void);		//	Starts object temperature reading (Zone 1)
		bool readTobj2(void);		//	Starts object temperature reading (Zone 2)
		bool readFullTemp(void);	//	Starts Ambient + Obj Temp Reading (+ Obj 2 if [m_dualZoneFlag])
		float getTamb(void) const;	//	Returns ambient temperature
		float getTobj(void) const;	//	Returns object temperature (Zone 1)
		float getTobj2(void) const;	//	Returns object temperature (Zone 2)

		bool readRawIR(uint8_t channel);			//	Starts Raw IR Temp Reading
		int16_t getRawIR(uint8_t channel) const;	//	Returns Raw IR Temp

		//	Emissivity: The New Value is Used After a Power Cycle of the Sensor
		bool readEmissivity(void);		//	Starts Emissivity Reading from EEPROM
		float getEmissivity() const;	//	Returns [m_emissivityRaw] / 0xFFFF.0f

		//	Config: The New Value is Used After a Power Cycle of the Sensor
		bool readConfig(void);	//	Starts Config Register 1 Reading from EEPROM
		MLX90614::IIR_t getIIR(void) const;	//	Returns Current IIR Percentage
		uint16_t getFIRsamples(void) const;	//	Returns Current FIR Samples
		float getGain(void) const;			//	Returns Amplifier Gain (Not Modifiable)
		bool isDualZone(void) const;		//	Returns [m_dualFlag]
		bool hasTwoChannels(void) const;	//	Returns [m_twoChannelsFlag]
		bool setFilter(IIR_t iirPercentage);//	Starts Config Register 1 Writing
		bool setFilter(FIR_t firSamples);	//	Starts Config Register 1 Writing

		//	Slave Address: Use Only ONE Sensor on the Bus for this. Valid After a Power Cycle of the Sensor
		bool setBusAddress(uint8_t newAddress);	//	0x1 ... 0x7F
		void updateBussAddress(uint8_t newAddress);	//	[m_addr] = newAddress

		//	RAW EEPROM Access: PWM, Thermal Relay, Ranges, etc
		bool readEEPROM(eepromAddress_t eepromAddress);	//	Starts EEPROM Reading
		uint16_t getEEPROMdata() const;					//	Returns [m_eepromData]
		bool writeEEPROM(eepromAddress_t eepromAddress, uint16_t value);	//	Starts EEPROM Writing
		static uint16_t temp2raw(float celsius);	//	EEPROM Encoding for Tomin, Tomax, Relay Threshold, etc

		//	Sleep Mode (Just for 3V Parts)
		bool sleep(void);				//	Starts Sleep Job
		void setAwake(void);			//	[m_sleepingFlag] = false
		bool isSleeping(void) const;	//	Returns [m_sleepingFlag]

		bool isBusy(void) const;	//	Returns [m_mlxStatus] == BUSY
		bool isDone(void) const;	//	Returns [m_mlxStatus] == OP_DONE
		bool hasError(void) const;	//	Returns [m_mlxStatus] == ERR_XXXX
		MLX90614::mlxStatus_t getStatus(void) const;	//	Returns [m_mlxStatus]
		I2C::i2cStatus_t getI2Cstatus(void) const;		//	Returns [m_i2cStatus]

		~MLX90614();	//	Destructor
};

#endif /* MLX90614_H_ */
