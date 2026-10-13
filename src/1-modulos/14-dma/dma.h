/*
 * dma.h
 *
 *  Created on: 10 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#ifndef DMA_H_
#define DMA_H_

#include "inmux.h"

#if defined (__cplusplus)
extern "C" {

void DMA_IRQHandler(void);

}
#endif

class Dma{
	public:
		static const uint8_t DMA_CHANNELS = 25;
		static const uint16_t MAX_XFER_WORDS = 1024;

		enum wordWidth_t : uint8_t{
			WIDTH_8BIT = 0,
			WIDTH_16BIT,
			WIDTH_32BIT
		};

		enum addrInc_t : uint8_t{
			INC_NONE = 0,	//	Fixed Address (Peripheral Data Register)
			INC_1 = 1,		//	Address Increment: 1 * wordWidth
			INC_2 = 2,		//	Address Increment: 2 * wordWidth
			INC_4 = 4		//	Address Increment: 4 * wordWidth
		};

		enum trigPolarity_t : uint8_t{
			TRIGPOL_LOW_FALLING = 0,
			TRIGPOL_HIGH_RISING
		};

		enum trigType_t : uint8_t{
			TRIGTYPE_EDGE = 0,
			TRIGTYPE_LEVEL
		};

		enum xferOption_t : uint8_t{
			OPT_NONE = 0,
			OPT_INTA = (1 << 0),	//	Set INTA when the Descriptor is Exhausted
			OPT_INTB = (1 << 1),	//	Set INTB when the Descriptor is Exhausted
			OPT_CLRTRIG = (1 << 2)	//	Clear the Trigger when the Descriptor is Exhausted
		};

		struct Descriptor{
			uint32_t xfercfg;	//	XFERCFG
			uint32_t srcEnd;	//	Addr of the Last Source Item
			uint32_t dstEnd;	//	Addr of the Last Destination Item
			uint32_t next;		//	Next Descriptor Addr (When Reload == 1)
		} __attribute__((aligned(16)));	//	Forces the Compiler to Place the Structure at a Memory Address that is a Multiple of 16

		static Descriptor descriptorsTable[Dma::DMA_CHANNELS] __attribute__((aligned(512)));		//	Forces the Compiler to Place the Vector at a Memory Address that is a Multiple of 512
		static volatile uint8_t doneAchannels[Dma::DMA_CHANNELS];	//	Contains All Channels With INTA Active
		static volatile uint8_t doneBchannels[Dma::DMA_CHANNELS];	//	Contains All Channels With INTB Active
		static volatile uint8_t errorChannels[Dma::DMA_CHANNELS];	//	Contains All Channels With Errors

		static volatile uint32_t irqCounter;	//	[DMA TEST] Number of ISR Entries

	private:
		//	CFG
		static const uint8_t CFG_PERIPHREQEN = (1 << 0);
		static const uint8_t CFG_HWTRIGEN = (1 << 1);
		static const uint8_t CFG_TRIGPOL = (1 << 4);
		static const uint8_t CFG_TRIGTYPE = (1 << 5);
		static const uint8_t CFG_TRIGBURST = (1 << 6);
		static const uint16_t CFG_BURSTPWR = (0xF << 8);
		static const uint16_t CFG_SRCBURSTWRAP = (1 << 14);
		static const uint16_t CFG_DSTBURSTWRAP = (1 << 15);
		static const uint32_t CFG_CHPRIORITY = (0x7 << 16);
		//	XFERCFG
		static const uint8_t X_CFGVALID = (1 << 0);
		static const uint8_t X_RELOAD = (1 << 1);
		static const uint8_t X_SWTRIG = (1 << 2);
		static const uint8_t X_CLRTRIG = (1 << 3);
		static const uint8_t X_SETINTA = (1 << 4);
		static const uint8_t X_SETINTB = (1 << 5);

		uint8_t m_channel;
		uint32_t m_cfg;		//	Channel CFG Register
		uint32_t m_xfercfg;	//	Transfer Configuration of the First Descriptor
		bool m_readyFlag;
		bool m_runningFlag;
		bool m_pingpongFlag;	//	If Ping-Pong -> INTA / INTB Do Not Mean "Finished"
		bool m_wasActiveFlag;	//	True if ACTIVE0 Sets "1" for this Channel (or Software Triggered)

		void setCFG(uint32_t mask, uint32_t value);	//	Builds [m_cfg]

		static void isrHandler(void);	//	DMA ISR Handler, static cause 25 channels and just 1 IRQ

	public:
		Dma(uint8_t channel, bool enableIRQ = false);	//	Constructor

		static void init(void);	//	Each Dma Class is a DMA Channel so Global Init is Needed

		static bool buildDescriptor(Descriptor& descriptor, const void *src, void *dst, uint16_t count,
									wordWidth_t width, addrInc_t srcInc, addrInc_t dstInc,
									uint8_t options = (xferOption_t::OPT_INTA | xferOption_t::OPT_CLRTRIG),
									const Descriptor* next = nullptr);	//	Returns False if Invalid Descriptor Config


		//	Trigger Source (Peripheral and Hardware Can be Combined)
		void setSoftwareTrigger(void);		//	Clears Peripheral Request and Hardware Trigger
		void setPeripheralRequest(void);	//	Sets PERIPHREQEN
		bool setHardwareTrigger(InMux::DMA_TRIGGER_INPUT_SOURCE_t source,
								trigPolarity_t polarity = trigPolarity_t::TRIGPOL_HIGH_RISING,
								trigType_t type = trigType_t::TRIGTYPE_EDGE);	//	Configs Hardware Trigger
		bool setChannelTrigger(const Dma &master, InMux::DMA_TRIGGER_INPUT_NUMBER_t line,
							   trigPolarity_t polarity = trigPolarity_t::TRIGPOL_LOW_FALLING,
							   trigType_t type = trigType_t::TRIGTYPE_EDGE);	//	[master]'s Trigger Output -> This Channel Trigger Input

		//	Channel Settings
		bool setBurstMode(uint8_t sizePower);	//	One Trigger = One Burst of 2^sizePower Transfers
		void clearBurstMode(void);				//	One Trigger = One Transfer
		void setBurstWrap(bool src, bool dst);
		void clearBurstWrap(bool src, bool dst);
		bool setPriority(uint8_t priority);		//	0 = Highest

		//	Transfer
		bool setTransfer(const void *src, void *dst, uint16_t count, wordWidth_t width,
						 addrInc_t srcInc, addrInc_t dstInc, uint8_t options = (xferOption_t::OPT_INTA | xferOption_t::OPT_CLRTRIG),
						 const Descriptor *nextDescriptor = nullptr);
		bool startTransfer(void);	//	Writes CFG, Enables Channel and Validates Descriptor
		void setTrig(void);	//	SETTRIG0 Register

		void enableInt(void);		//	Sets INTENSET0
		void disableInt(void);		//	Sets INTENCLR0
		void enableNVIC_int(void);	//	Enables NVIC Interrupt for DMA
		void disableNVIC_int(void);	//	Disables NVIC Interrupt for DMA

		bool aIsDone(void) const;	//	True if INTA was Set by the Last Descriptor
		bool bIsDone(void) const;	//	True if INTB was Set by the Last Descriptor
		bool hasError(void) const;	//	True if Error Occurred
		bool isBusy(void);			//	Returns True if Transfer in Progress

		void clearFlags(void);		//	Clears INTSTAT
		void clearAflags(void);		//	Clears Only INTA (PingPong: Buffer A Exhausted)
		void clearBflags(void);		//	Clears Only INTB (PingPong: Buffer B Exhausted)

		void requestStop(void);		//	Step 1: Disable Channel
		bool stopDone(void);		//	Step 2: True if Disabled

		uint8_t getChannel(void) const;	//	Returns [m_channel]

		friend void DMA_IRQHandler(void);	//	ISR Handler for DMA

		~Dma();	//	Destructor
};

class DmaTest{
	public:
		enum testBit_t : uint8_t{
			T_MEM32 = (1 << 0),
			T_FILL8 = (1 << 1),
			T_PP_DATA = (1 << 2),
			T_PP_BUSY = (1 << 3),
			T_STOP = (1 << 4),
			T_ALL = 0x1F
		};

	private:
		enum state_t : uint8_t{
			S_IDLE = 0,
			S_T1_START,
			S_T1_WAIT,
			S_T2_START,
			S_T2_WAIT,
			S_T3_START,
			S_T3_WAIT,
			S_T3_STOPPING,
			S_RECOVER,		//	Timeout: Stop the Channel and Continue with Next State
			S_FINISHED
		};

		Dma m_dma;

		volatile state_t m_state;
		state_t m_nextState;

		uint32_t m_mainLoops;	//	Times stateMachine() executed

		volatile uint8_t m_passMask;
		volatile uint8_t m_failMask;

		uint8_t m_exhaustedBuffCount;	//	Ping-Pong Buffer Consumed

		bool m_ppDataOkFlag;
		bool m_ppBusyOkFlag;

		void report(uint8_t bit, bool ok);	//	Builds [m_passMask] and [m_failMask]
		void recover(state_t nextState);

	public:
		DmaTest(uint8_t channel = 22);	//	Software Triggered

		void init(void);	//	Start the tests

		void stateMachine(void);	//	Test Handler

		bool hasFinished(void) const;	//	True if Test Finished
		bool allPassed(void) const;		//	True if Finished and Every Test Passed
		uint8_t getPassMask(void) const;//	Allows Watching Test in Debugger
		uint8_t getFailMask(void) const;//	Idem

		~DmaTest();	//	Destructor

};

class DmaChainTest{
	public:
		enum testBit_t : uint8_t{
			T_X_COPY = (1 << 0),	//	Channel X Copied srcX -> bufX
			T_Y_CHAIN = (1 << 1),	//	Channel Y Was Triggered by X and Copied Whole Buffer
			T_ALL = 0x03
		};

	private:
		enum state_t : uint8_t{
			S_IDLE = 0,
			S_START,
			S_WAIT,
			S_STOPPING,
			S_FINISHED
		};

		Dma m_channelX;
		Dma m_channelY;

		volatile state_t m_state;
		volatile uint8_t m_passMask;
		volatile uint8_t m_failMask;
		volatile uint8_t m_copiedWords;

		uint32_t m_mainLoops;

		bool m_chXstopped;
		bool m_chYstopped;

		void report(testBit_t bit, bool ok);
		void evaluate(void);

	public:
		DmaChainTest(uint8_t channelX = 20, uint8_t channelY = 21);	//	Constructor

		void init(void);	//	Dma::init() HAS To Be Called Before

		void stateMachine(void);	//	Handles Test

		bool hasFinished(void) const;		//	True if Test Finished
		bool allPassed(void) const;			//	True if Finished and Every Test Passed
		uint8_t getPassMask(void) const;	//	Allows Watching Test in Debugger
		uint8_t getFailMask(void) const;	//	Idem
		uint8_t getCopiedWords(void) const;	//	Idem

		~DmaChainTest();	//	Destructor
};

class DmaTest2{
	public:
		enum testBit_t : uint8_t{
			T_LIMITS = (1 << 0),
			T_WIDTHS = (1 << 1),
			T_LINEAR = (1 << 2),
			T_ABORT = (1 << 3),
			T_RESTART = (1 << 4),
			T_REUSE = (1 << 5),
			T_PP2SINGLE = (1 << 6),
			T_ALL = 0x7F
		};

		static const uint16_t REUSE_ITERATIONS = 1000;

	private:
		enum state_t : uint8_t{
			S_IDLE = 0,
			S_LIMITS_REJECT, S_LIMITS_START, S_LIMITS_WAIT,
			S_WIDTHS_START, S_WIDTHS_WAIT,
			S_LINEAR_START, S_LINEAR_WAIT,
			S_ABORT_START, S_ABORT_STOPPING,
			S_RESTART_START, S_RESTART_WAIT,
			S_REUSE_START, S_REUSE_WAIT,
			S_PP_START, S_PP_WAIT, S_PP_STOPPING,
			S_SINGLE_START, S_SINGLE_WAIT,
			S_RECOVER,
			S_FINISHED
		};

		Dma m_dma;

		volatile state_t m_state;
		state_t m_stateAfterRecover;

		volatile uint8_t m_passMask;
		volatile uint8_t m_failMask;
		volatile uint8_t m_firstFailId;
		volatile uint16_t m_abortCopied;
		volatile uint16_t m_reuseFailIter;

		uint32_t m_mainLoops;
		uint16_t m_iterationNumber;
		uint8_t m_subStep;
		bool m_okAccumFlag;	//	Accumulates the Checks of the Last in Progress
		bool m_abortOkFlag;

		void report(testBit_t bit, bool ok);
		void check(uint8_t id, bool cond);
		void nextTest(state_t nextState);	//	Resets Sub Step, Iteration Counter and Accumulator
		void finishTest(testBit_t bit, state_t nextState);		//	Reports Accumulator and Goes Next
		void timeoutRecover(testBit_t bit, state_t nextState);	//	Reports Failure, Stops Channel and Goes Next
		bool fireTest(const void *src, void *dst, uint16_t count, Dma::wordWidth_t width, Dma::addrInc_t srcInc, Dma::addrInc_t dstInc, uint8_t options = (Dma::OPT_INTA | Dma::OPT_CLRTRIG), const Dma::Descriptor *next = nullptr);

	public:
		DmaTest2(uint8_t channel = 19);	//	Constructor

		void init(void);	//	Starts Testing (Dma::init() HAS To Be Called Before)

		void stateMachine(void);	//	Test Handler

		bool hasFinished(void) const;
		bool allPassed(void) const;
		uint8_t getPassMask(void) const;
		uint8_t getFailMask(void) const;
		uint8_t getFirstFailId(void) const;	//	0: No Failure
		uint16_t getAbortCopied(void) const;//	Bytes Already Copied When Abort Took Effect (ideally 0)
		uint16_t getReuseFailIteration(void) const;	//	0xFFFF: No Failure

		~DmaTest2();	//	Destructor
};

class DmaIrqTest{
	public:
		enum testBit_t : uint8_t{
			T_SINGLE = (1 << 0),	//	1 Transfer with Interrupt: ISR Entered Once, INTA0 Cleared by ISR, aIsDone() Works and clearFlags() Too
			T_BOTH = (1 << 1),		//	INTA | INTB On the Same Descriptor: ISR Entered Once, Both Flags Handled and Cleared
			T_LATE = (1 << 2),		//	Ping-Pong: Buffer A Not Cleared Before Buffer B Finishes, Both Flags Have to Survive (2 ISR Entries, 2 Flags)
			T_MANY = (1 << 3),		//	200 Transfers: 200 ISR Entries (Check Lost / Extra)
			T_NOINT = (1 << 4),		//	Channel Int Disabled: ISR Has No Entries, Polling Still Should Work
			T_TWOCH = (1 << 5),		//	Chanel A Interrupt, Channel B Int Disabled: ISR Should Not Clear Bs Flag
			T_ALL = 0x3F
		};

	private:
		enum state_t : uint8_t{
			S_IDLE = 0,
			S_SINGLE_START, S_SINGLE_WAIT,
			S_BOTH_START, S_BOTH_WAIT,
			S_LATE_START, S_LATE_WAIT, S_LATE_STOPPING,
			S_MANY_START, S_MANY_WAIT,
			S_NOINT_START, S_NOINT_WAIT,
			S_TWOCH_START, S_TWOCH_WAIT,
			S_RECOVER,
			S_FINISHED
		};

		Dma m_dmaA;
		Dma m_dmaB;
		volatile state_t m_state;
		state_t m_recoverNext;
		volatile uint8_t m_pass;
		volatile uint8_t m_fail;
		volatile uint8_t m_firstFailId;
		uint32_t m_loops;
		uint32_t m_irqBase;		//	Dma::irqCount When the Transfer Was Started
		uint16_t m_settle;		//	Passes Waited After the ISR Ran, to Catch Extra (Spurious) ISR Entries
		uint16_t m_iter;
		uint8_t m_sub;
		bool m_okAccFlag;

		void report(testBit_t bit, bool ok);
		void check(uint8_t id, bool cond);
		void nextTest(state_t nextState);
		void finishTest(testBit_t bit, state_t nextState);
		void timeoutRecover(testBit_t bit, state_t nextState);
		bool fireTest(Dma &dma, const void *src, void *dst, uint16_t count, Dma::wordWidth_t width, Dma::addrInc_t srcInc, Dma::addrInc_t dstInc, uint8_t options = (Dma::OPT_INTA | Dma::OPT_CLRTRIG), const Dma::Descriptor *next = nullptr);
		bool hwA(const Dma &dma) const;		//	INTA0 Register Bit of the Channel (Not the Latched Flag)
		bool hwB(const Dma &dma) const;
		uint32_t irqDelta(void) const;

	public:
		DmaIrqTest(uint8_t channelA = 18, uint8_t channelB = 17);	//	Constructor

		void init(void);	//	Enables DMA Interrupts in NVIC

		void stateMachine(void);	//	Test Handler

		bool hasFinished(void) const;
		bool allPassed(void) const;
		uint8_t getPassMask(void) const;
		uint8_t getFailMask(void) const;
		uint8_t getFirstFailId(void) const;
		uint32_t getIrqCount(void) const;

		~DmaIrqTest();	//	Destructor
};

#endif /* DMA_H_ */
