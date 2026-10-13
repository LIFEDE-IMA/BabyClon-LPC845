/*
 * dma.cpp
 *
 *  Created on: 10 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "dma.h"

Dma::Descriptor Dma::descriptorsTable[Dma::DMA_CHANNELS] __attribute__((aligned(512)));
volatile uint8_t Dma::doneAchannels[Dma::DMA_CHANNELS] = {0};
volatile uint8_t Dma::doneBchannels[Dma::DMA_CHANNELS] = {0};
volatile uint8_t Dma::errorChannels[Dma::DMA_CHANNELS] = {0};

void Dma::init(void){
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 29);	//	Enables DMA Clk
	SYSCON->PRESETCTRL0 &= ~(1 << 29);		//	Reset DMA Peripheral
	SYSCON->PRESETCTRL0 |= (1 << 29);		//	Clr Reset

	DMA->SRAMBASE = (uint32_t)Dma::descriptorsTable;	//	Address Bits of the Beginning of the DMA Descriptor Table

	DMA->CTRL |= 0x1;	//	DMA Controller Enabled
}

bool Dma::buildDescriptor(Descriptor& descriptor, const void *src, void *dst, uint16_t count, wordWidth_t width, addrInc_t srcInc, addrInc_t dstInc, uint8_t options, const Descriptor* next){
	if((count == 0) || (count > Dma::MAX_XFER_WORDS))	return false;

	uint32_t wBytes = (1 << width);
	uint32_t source = (uint32_t)src;
	uint32_t dest = (uint32_t)dst;
	uint32_t link = (uint32_t)next;

	if((source & (wBytes - 1)) || (dest & (wBytes - 1)))	return false;	//	Addresses Aligned to the Width
	if(link & 0xF)	return false;	//	Descriptors Have to Be 16-byte Aligned

	//	END Addresses
	descriptor.srcEnd = (source + (((uint32_t)(count - 1)) * ((uint32_t)srcInc) * wBytes));
	descriptor.dstEnd = (dest + (((uint32_t)(count - 1)) * ((uint32_t)dstInc) * wBytes));
	descriptor.next = link;

	uint8_t sourceInc = srcInc;
	uint8_t destInc = dstInc;
	if(srcInc == addrInc_t::INC_4)	sourceInc = 0x3;
	if(dstInc == addrInc_t::INC_4)	destInc = 0x3;


	descriptor.xfercfg = (Dma::X_CFGVALID
					   | (next ? Dma::X_RELOAD : 0)	//	Reload Only If Ping-Pong
					   | ((options & xferOption_t::OPT_CLRTRIG) ? Dma::X_CLRTRIG : 0)
					   | ((options & xferOption_t::OPT_INTA) ? Dma::X_SETINTA : 0)
					   | ((options & xferOption_t::OPT_INTB) ? Dma::X_SETINTB : 0)
					   | (width << 8)
					   | (sourceInc << 12)
					   | (destInc << 14)
					   | ((count - 1) << 16));	//	Number of Transfers to Be Performed is (XFERCOUNT + 1) * WIDTH

	return true;
}

Dma::Dma(uint8_t channel, bool enableIRQ){
	m_channel = ((channel < Dma::DMA_CHANNELS) ? channel : 0);
	m_cfg = 0;
	m_xfercfg = 0;
	m_readyFlag = false;
	m_runningFlag = false;
	m_pingpongFlag = false;
	m_wasActiveFlag = false;

	Dma::descriptorsTable[m_channel].xfercfg = 0;
	Dma::descriptorsTable[m_channel].srcEnd = 0;
	Dma::descriptorsTable[m_channel].dstEnd = 0;
	Dma::descriptorsTable[m_channel].next = 0;

	if(enableIRQ)	Dma::enableNVIC_int();
}

void Dma::setCFG(uint32_t mask, uint32_t value){
	m_cfg = ((m_cfg & ~mask) | (value & mask));
}

void Dma::setSoftwareTrigger(void){
	//	Disables PERIPHREQEN and HWTRIGEN, TRIGPOL = Falling Edge, TRIGTYPE = Edge
	Dma::setCFG((Dma::CFG_PERIPHREQEN | Dma::CFG_HWTRIGEN | Dma::CFG_TRIGPOL | Dma::CFG_TRIGTYPE), 0);
}

void Dma::setPeripheralRequest(void){
	//	Enables PERIPHREQEN
	Dma::setCFG(Dma::CFG_PERIPHREQEN, Dma::CFG_PERIPHREQEN);
}

bool Dma::setHardwareTrigger(InMux::DMA_TRIGGER_INPUT_SOURCE_t source, trigPolarity_t polarity, trigType_t type){
	if(Dma::isBusy())	return false;

	InMux::setDMA_TriggerInput(m_channel, source);	//	Sets [source] as Trigger Input for this DMA Channel
	uint32_t value = Dma::CFG_HWTRIGEN;
	value |= ((polarity == trigPolarity_t::TRIGPOL_HIGH_RISING) ? Dma::CFG_TRIGPOL : 0);
	value |= ((type == trigType_t::TRIGTYPE_LEVEL) ? Dma::CFG_TRIGTYPE : 0);
	//	Enables HWTRIGEN, Sets TRIGPOL = [polarity] and TRIGTYPE = [type]
	Dma::setCFG((Dma::CFG_HWTRIGEN | Dma::CFG_TRIGPOL | Dma::CFG_TRIGTYPE), value);

	return true;
}

bool Dma::setChannelTrigger(const Dma &master, InMux::DMA_TRIGGER_INPUT_NUMBER_t line, trigPolarity_t polarity, trigType_t type){
	if(Dma::isBusy())	return false;

	InMux::setDMA_INMUX(line, master.m_channel);	//	Trigger Output of [master] -> DMA_INMUX[line]
	InMux::DMA_TRIGGER_INPUT_SOURCE_t inputSource = ((line == InMux::TRIGGER_INPUT_11) ? InMux::DMA_INMUX0 : InMux::DMA_INMUX1);
	return Dma::setHardwareTrigger(inputSource, polarity, type);
}

bool Dma::setBurstMode(uint8_t sizePower){
	if(sizePower > 10)	return false;
	//	Enables Burst Transfer and Sets BURSTPOWER (2^sizePower)
	Dma::setCFG((Dma::CFG_TRIGBURST | Dma::CFG_BURSTPWR), (Dma::CFG_TRIGBURST | (sizePower << 8)));
	return true;
}

void Dma::clearBurstMode(void){
	//	Disables Burst Transfer and Sets BURSTPOWER in 1
	Dma::setCFG((Dma::CFG_TRIGBURST | Dma::CFG_BURSTPWR), 0);
}

void Dma::setBurstWrap(bool src, bool dst){
	uint32_t value = ((src ? Dma::CFG_SRCBURSTWRAP : 0) | (dst ? Dma::CFG_DSTBURSTWRAP : 0));
	//	Enables Destination and/or Source Data Addr Wrap
	Dma::setCFG((Dma::CFG_SRCBURSTWRAP | Dma::CFG_DSTBURSTWRAP), value);
}

void Dma::clearBurstWrap(bool src, bool dst){
	uint32_t mask = ((src ? Dma::CFG_SRCBURSTWRAP : 0) | (dst ? Dma::CFG_DSTBURSTWRAP : 0));
	//	Disables Destination and/or Source Data Addr Wrap
	Dma::setCFG(mask, 0);
}

bool Dma::setPriority(uint8_t priority){
	if(priority > 0x7)	return false;

	Dma::setCFG(Dma::CFG_CHPRIORITY, (priority << 16));
	return true;
}

bool Dma::setTransfer(const void *src, void *dst, uint16_t count, wordWidth_t width, addrInc_t srcInc, addrInc_t dstInc, uint8_t options, const Descriptor *nextDescriptor){
	if(Dma::isBusy())	return false;

	Dma::Descriptor descriptor;

	m_readyFlag = false;

	if(!(Dma::buildDescriptor(descriptor, src, dst, count, width, srcInc, dstInc, options, nextDescriptor)))
		return false;

	Dma::descriptorsTable[m_channel].xfercfg = 0;	//	Reserved
	Dma::descriptorsTable[m_channel].srcEnd = descriptor.srcEnd;
	Dma::descriptorsTable[m_channel].dstEnd = descriptor.dstEnd;
	Dma::descriptorsTable[m_channel].next = descriptor.next;
	m_xfercfg = descriptor.xfercfg;

	m_pingpongFlag = (nextDescriptor != nullptr);	//	Buffer A + Buffer B
	m_readyFlag = true;
	return true;
}

bool Dma::startTransfer(void){
	if(!m_readyFlag || Dma::isBusy())	return false;

	m_runningFlag = false;

	Dma::clearFlags();
	DMA->ENABLECLR0 = (1 << m_channel);	//	Disable Channel
	DMA->CHANNEL[m_channel].CFG = m_cfg;	//	Load CFG (Builded with setCFG())
	DMA->ENABLESET0 = (1 << m_channel);	//	Enable Channel

	uint32_t xfercfg = m_xfercfg;
	if(!(m_cfg & (Dma::CFG_PERIPHREQEN | Dma::CFG_HWTRIGEN)))
		xfercfg |= Dma::X_SWTRIG;	//	If Was Not Configured as Peripheral or Hardware Triggered -> Software Triggered

	m_runningFlag = true;
	m_wasActiveFlag = !(m_cfg & (Dma::CFG_PERIPHREQEN | Dma::CFG_HWTRIGEN));	//	If Sw Triggered -> Start Right Now

	DMA->CHANNEL[m_channel].XFERCFG &= ~((0x1F << 1) | (0x3 << 8) | (0x3FFF << 12));	//	Clears Register
	DMA->CHANNEL[m_channel].XFERCFG |= xfercfg;

	return true;
}

void Dma::setTrig(void){
	DMA->SETTRIG0 = (1 << m_channel);	//	Triggered, the Trigger for this DMA Channel is Set. DMA Ops Will Be Carried Out
}

void Dma::enableInt(void){
	DMA->INTENSET0 = (1 << m_channel);
}

void Dma::disableInt(void){
	DMA->INTENCLR0 = (1 << m_channel);
}

void Dma::enableNVIC_int(void){
	NVIC->ISER[0] = (1 << DMA0_IRQn);
}

void Dma::disableNVIC_int(void){
	NVIC->ICER[0] = (1 << DMA0_IRQn);
}

bool Dma::aIsDone(void) const{
	uint32_t intA = DMA->INTA0;
	return ((intA & (1 << m_channel)) || Dma::doneAchannels[m_channel]);
}

bool Dma::bIsDone(void) const{
	uint32_t intB = DMA->INTB0;
	return ((intB & (1 << m_channel)) || Dma::doneBchannels[m_channel]);
}

bool Dma::hasError(void) const{
	uint32_t errInt = DMA->ERRINT0;
	return ((errInt & (1 << m_channel)) || Dma::errorChannels[m_channel]);
}

bool Dma::isBusy(void){
	if(!m_runningFlag)	return false;

	if(DMA->ACTIVE0 & (1 << m_channel)){
		m_wasActiveFlag = true;
		return true;
	}

	//	Not Active: Finished (was active before), Failed, or Single Descriptor (not pingpong) INTA / INTB Fired
	if(m_wasActiveFlag || Dma::hasError() || (!m_pingpongFlag && (Dma::aIsDone() || Dma::bIsDone())))
		m_runningFlag = false;

	return m_runningFlag;
}

void Dma::clearFlags(void){
	DMA->INTA0 = (1 << m_channel);
	DMA->INTB0 = (1 << m_channel);
	DMA->ERRINT0 = (1 << m_channel);
	Dma::doneAchannels[m_channel] = 0;
	Dma::doneBchannels[m_channel] = 0;
	Dma::errorChannels[m_channel] = 0;
}

void Dma::clearAflags(void){
	DMA->INTA0 = (1 << m_channel);
	Dma::doneAchannels[m_channel] = 0;
}

void Dma::clearBflags(void){
	DMA->INTB0 = (1 << m_channel);
	Dma::doneBchannels[m_channel] = 0;
}

void Dma::requestStop(void){
	DMA->ENABLECLR0 = (1 << m_channel);	//	Disables Channel
}

bool Dma::stopDone(void){
	if(DMA->BUSY0 & (1 << m_channel))
		return false;

	DMA->ABORT0 = (1 << m_channel);	//	Abort Operation After Disabling

	m_runningFlag = false;
	return true;
}

uint8_t Dma::getChannel(void) const{ return m_channel; }

void Dma::isrHandler(void){
	uint32_t intA = DMA->INTA0;	//	If Bit n = 1 -> Channel n has Interrupt A Active
	uint32_t intB = DMA->INTB0;	//	Idem for Interrupt B
	uint32_t err = DMA->ERRINT0;//	Idem for Error Interrupt

	//	Clear Ints
	DMA->INTA0 = (intA & ~(0x7F << 25));	//	Just in Case Reserved Had "ones"
	DMA->INTB0 = (intB & ~(0x7F << 25));	//	Idem
	DMA->ERRINT0 = (err & ~(0x7F << 25));	//	Idem

	for(uint8_t channel = 0; channel < Dma::DMA_CHANNELS; channel++){
		uint32_t n = (1 << channel);
		if(intA & n)	Dma::doneAchannels[channel] = 1;
		if(intB & n)	Dma::doneBchannels[channel] = 1;
		if(err & n)		Dma::errorChannels[channel] = 1;
	}
}

void DMA_IRQHandler(void){
	Dma::isrHandler();
}

Dma::~Dma(){}

namespace{
	//	Static, Not Members -> The Alignment of the Descriptors Does Not Depend on Where a DmaTest Object is Placed
	const uint32_t TIMEOUT_LOOPS = 200000;
	const uint8_t PP_WORDS = 16;
	const uint8_t PP_BUFFERS = 4;	//	A, B, A, B

	uint32_t s_src32[32];
	uint32_t s_dst32[32];

	uint8_t s_srcByte;
	uint8_t s_dst8[18];	//	16 bytes + 2 sentinels

	uint32_t s_ppSrc_[PP_WORDS];
	uint32_t s_ppA_[PP_WORDS];
	uint32_t s_ppB_[PP_WORDS];

	Dma::Descriptor s_buffA;
	Dma::Descriptor s_buffB;
}

DmaTest::DmaTest(uint8_t channel) : m_dma(channel){
	m_state = state_t::S_IDLE;
	m_nextState = state_t::S_IDLE;
	m_passMask = 0;
	m_failMask = 0;
	m_exhaustedBuffCount = 0;
	m_mainLoops = 0;
	m_ppDataOkFlag = true;
	m_ppBusyOkFlag = true;
}

void DmaTest::init(void){
	Dma::init();
	m_passMask = 0;
	m_failMask = 0;
	m_state = state_t::S_T1_START;
}

bool DmaTest::hasFinished(void) const{ return (m_state == state_t::S_FINISHED); }

bool DmaTest::allPassed(void) const{ return (DmaTest::hasFinished() && (m_passMask == testBit_t::T_ALL) && (m_failMask == 0)); }

uint8_t DmaTest::getPassMask(void) const{ return m_passMask; }

uint8_t DmaTest::getFailMask(void) const{ return m_failMask; }

void DmaTest::report(uint8_t bit, bool ok){
	if(ok)	m_passMask |= bit;
	else	m_failMask |= bit;
}

void DmaTest::recover(state_t nextState){
	m_dma.requestStop();
	m_nextState = nextState;
	m_state = state_t::S_RECOVER;
}

void DmaTest::stateMachine(void){
	switch(m_state){
		case state_t::S_IDLE:
		case state_t::S_FINISHED:
			//	Nothing To Do
			break;

		//	--------------	TEST 1: 32 words, memory -> memory	--------------	//
		case state_t::S_T1_START:
			for(uint8_t idx = 0; idx < 32; idx++){
				s_src32[idx] = (0xA5A50000u + idx);
				s_dst32[idx] = 0;
			}
			m_mainLoops = 0;
			if(m_dma.setTransfer(s_src32, s_dst32, 32, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1) && m_dma.startTransfer()){
				m_state = state_t::S_T1_WAIT;
			}else{
				DmaTest::report(testBit_t::T_MEM32, false);
				m_state = state_t::S_T2_START;
			}
			break;

		case state_t::S_T1_WAIT:
			if(!m_dma.isBusy()){
				bool f_ok = (m_dma.aIsDone() && !m_dma.hasError());
				for(uint8_t idx = 0; idx < 32; idx++)
					if(s_dst32[idx] != s_src32[idx])
						f_ok = false;
				m_dma.clearFlags();
				DmaTest::report(testBit_t::T_MEM32, f_ok);
				m_state = state_t::S_T2_START;
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest::report(testBit_t::T_MEM32, false);
				DmaTest::recover(state_t::S_T2_START);
			}
			break;

		//	--------------	TEST 2: fixed source byte, 16 bytes memory -> memory	--------------	//
		case state_t::S_T2_START:
			s_srcByte = 0x5A;
			for(uint8_t idx = 0; idx < 18; idx++)
				s_dst8[idx] = ((idx < 16) ? 0x00 : 0xEE);
			m_mainLoops = 0;
			if(m_dma.setTransfer(&s_srcByte, s_dst8, 16, Dma::WIDTH_8BIT, Dma::INC_NONE, Dma::INC_1) && m_dma.startTransfer()){
				m_state = state_t::S_T2_WAIT;
			}else{
				DmaTest::report(testBit_t::T_FILL8, false);
				m_state = state_t::S_T3_START;
			}
			break;

		case state_t::S_T2_WAIT:
			if(!m_dma.isBusy()){
				bool f_ok = (m_dma.aIsDone() && !m_dma.hasError());
				for(uint8_t idx = 0; idx < 16; idx++)
					if(s_dst8[idx] != 0x5A)
						f_ok = false;
				if((s_dst8[16] != 0xEE) || (s_dst8[17] != 0xEE))
					f_ok = false;
				m_dma.clearFlags();
				DmaTest::report(testBit_t::T_FILL8, f_ok);
				m_state = state_t::S_T3_START;
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest::report(testBit_t::T_FILL8, false);
				DmaTest::recover(state_t::S_T3_START);
			}
			break;

		//	--------------	TEST 3: Ping-Pong, A/B/A/B memory -> memory	--------------	//
		case state_t::S_T3_START:
			for(uint8_t idx = 0; idx < PP_WORDS; idx++){
				s_ppSrc_[idx] = (0x1000u + idx);
				s_ppA_[idx] = 0;
				s_ppB_[idx] = 0;
			}
			m_exhaustedBuffCount = 0;
			m_ppDataOkFlag = true;
			m_ppBusyOkFlag = true;
			m_mainLoops = 0;
			if(m_dma.buildDescriptor(s_buffA, s_ppSrc_, s_ppA_, PP_WORDS, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTA | Dma::OPT_CLRTRIG), &s_buffB)
			&& m_dma.buildDescriptor(s_buffB, s_ppSrc_, s_ppB_, PP_WORDS, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTB | Dma::OPT_CLRTRIG), &s_buffA)
			&& m_dma.setTransfer(s_ppSrc_, s_ppA_, PP_WORDS, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTA | Dma::OPT_CLRTRIG), &s_buffB)
			&& m_dma.startTransfer()){
				m_state = state_t::S_T3_WAIT;
			}else{
				DmaTest::report(testBit_t::T_PP_DATA, false);
				DmaTest::report(testBit_t::T_PP_BUSY, false);
				DmaTest::report(testBit_t::T_STOP, false);
				m_state = state_t::S_FINISHED;
			}
			break;

		case state_t::S_T3_WAIT:{
			bool f_expectA = !(m_exhaustedBuffCount & 0x1);
			bool f_ready = (f_expectA ? m_dma.aIsDone() : m_dma.bIsDone());

			if(f_ready){
				uint32_t *buf = (f_expectA ? s_ppA_ : s_ppB_);
				bool f_expectOther = (f_expectA ? m_dma.bIsDone() : m_dma.aIsDone());

				for(uint8_t idx = 0; idx < PP_WORDS; idx++){
					if(buf[idx] != s_ppSrc_[idx])
						m_ppDataOkFlag = false;
				}

				if(f_expectOther || m_dma.hasError())	m_ppDataOkFlag = false;

				if(!m_dma.isBusy())	m_ppBusyOkFlag = false;

				if(f_expectA)	m_dma.clearAflags();
				else	m_dma.clearBflags();

				for(uint8_t idx = 0; idx < PP_WORDS; idx++) buf[idx] = 0;

				m_exhaustedBuffCount++;
				m_mainLoops = 0;

				if(m_exhaustedBuffCount >= PP_BUFFERS){
					m_dma.requestStop();
					m_state = state_t::S_T3_STOPPING;
				}else{
					m_dma.setTrig();
				}
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				m_ppDataOkFlag = false;
				m_dma.requestStop();
				m_mainLoops = 0;
				m_state = state_t::S_T3_STOPPING;
			}
			break;
		}

		case state_t::S_T3_STOPPING:
			if(m_dma.stopDone()){
				DmaTest::report(testBit_t::T_STOP, !m_dma.isBusy());
				DmaTest::report(testBit_t::T_PP_DATA, (m_ppDataOkFlag && (m_exhaustedBuffCount >= PP_BUFFERS)));
				DmaTest::report(testBit_t::T_PP_BUSY, m_ppBusyOkFlag);
				m_dma.clearFlags();
				m_state = state_t::S_FINISHED;
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest::report(testBit_t::T_STOP, false);
				DmaTest::report(testBit_t::T_PP_DATA, false);
				DmaTest::report(testBit_t::T_PP_BUSY, m_ppBusyOkFlag);
				m_state = state_t::S_FINISHED;
			}
			break;

		//	---------------	TIMEOUT RECOVERY  -------------------
		case state_t::S_RECOVER:
			if(m_dma.stopDone()){
				m_dma.clearFlags();
				m_state = m_nextState;
			}
			break;

		default:
			m_state = state_t::S_FINISHED;
			break;
	}
}

DmaTest::~DmaTest(){}

namespace{
	const uint8_t WORDS = 16;

	uint32_t s_srcX[WORDS];
	uint32_t s_bufX[WORDS];
	uint32_t s_bufY[WORDS];
}

DmaChainTest::DmaChainTest(uint8_t channelX, uint8_t channelY) : m_channelX(channelX), m_channelY(channelY){
	m_state = state_t::S_IDLE;
	m_passMask = 0;
	m_failMask = 0;
	m_copiedWords = 0;
	m_mainLoops = 0;
	m_chXstopped = false;
	m_chYstopped = false;
}

void DmaChainTest::init(void){
	m_passMask = 0;
	m_failMask = 0;
	m_state = state_t::S_START;
}

bool DmaChainTest::hasFinished(void) const{ return (m_state == state_t::S_FINISHED); }

bool DmaChainTest::allPassed(void) const{ return ((m_state == state_t::S_FINISHED) && (m_passMask == testBit_t::T_ALL) && (m_failMask == 0)); }

uint8_t DmaChainTest::getPassMask(void) const{ return m_passMask; }

uint8_t DmaChainTest::getFailMask(void) const{ return m_failMask; }

uint8_t DmaChainTest::getCopiedWords(void) const{ return m_copiedWords; }

void DmaChainTest::report(testBit_t bit, bool ok){
	if(ok)	m_passMask |= bit;
	else	m_failMask |= bit;
}

void DmaChainTest::evaluate(void){
	bool f_chXok = (m_channelX.aIsDone() && !m_channelX.hasError());
	uint8_t copied = 0;

	for(uint8_t idx = 0; idx < WORDS; idx++){
		if(s_bufX[idx] != s_srcX[idx])	f_chXok = false;
		if(s_bufY[idx] == s_srcX[idx])	copied++;
	}
	m_copiedWords = copied;

	DmaChainTest::report(testBit_t::T_X_COPY, f_chXok);
	DmaChainTest::report(testBit_t::T_Y_CHAIN, ((copied == WORDS) && m_channelY.aIsDone() && !m_channelY.hasError()));
}

void DmaChainTest::stateMachine(void){
	switch(m_state){
		case state_t::S_IDLE:
		case state_t::S_FINISHED:
			//	Nothing to Do
			break;

		case state_t::S_START:
			for(uint8_t idx = 0; idx < WORDS; idx++){
				s_srcX[idx] = (0xC3C30000u + idx);
				s_bufX[idx] = 0;
				s_bufY[idx] = 0;
			}
			m_copiedWords = 0;
			m_mainLoops = 0;
			m_chXstopped = false;
			m_chYstopped = false;

			//	Channel Y Armed First, its Triggered by Channel X Output (DMA Trigger Mux 0)
			//	Burst of 2^4 = 16 Transfers
			//	Channel X, Triggered by Software. CLRTRIG When Exhausted -> Falling Edge Triggers Channel Y
			if(m_channelY.setChannelTrigger(m_channelX, InMux::TRIGGER_INPUT_11, Dma::TRIGPOL_LOW_FALLING, Dma::TRIGTYPE_EDGE)
			&& m_channelY.setBurstMode(4)
			&& m_channelY.setTransfer(s_bufX, s_bufY, WORDS, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1)
			&& m_channelY.startTransfer()
			&& m_channelX.setTransfer(s_srcX, s_bufX, WORDS, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1)
			&& m_channelX.startTransfer()){
				m_state = state_t::S_WAIT;
			}else{
				DmaChainTest::report(testBit_t::T_X_COPY, false);
				DmaChainTest::report(testBit_t::T_Y_CHAIN, false);
				m_state = state_t::S_FINISHED;
			}
			break;

		case state_t::S_WAIT:
			if(!m_channelX.isBusy() && !m_channelY.isBusy()){
				DmaChainTest::evaluate();
				m_channelX.clearFlags();
				m_channelY.clearFlags();
				m_state = state_t::S_FINISHED;
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaChainTest::evaluate();
				DmaChainTest::report(testBit_t::T_Y_CHAIN, false);
				m_channelX.requestStop();
				m_channelY.requestStop();
				m_mainLoops = 0;
				m_state = state_t::S_STOPPING;
			}
			break;

		case state_t::S_STOPPING:
			if(!m_chXstopped)	m_chXstopped = m_channelX.stopDone();
			if(!m_chYstopped)	m_chYstopped = m_channelY.stopDone();

			if((m_chXstopped && m_chYstopped) || (++m_mainLoops > TIMEOUT_LOOPS)){
				m_channelX.clearFlags();
				m_channelY.clearFlags();
				m_state = state_t::S_FINISHED;
			}
			break;

		default:
			m_state = state_t::S_FINISHED;
			break;
	}
}

DmaChainTest::~DmaChainTest(){}

/*
 * 									DMA TEST 2
 *
 *  Check ids (getFirstFailId()):
 *  				returned			meaning
 *      T_LIMITS       11 		   count 0 accepted
 *      			   12 		   count 1025 accepted
 *      			   13 		   misaligned src (32-bit) accepted
 *                     14 		   misaligned dst (16-bit)
 *                     15 		   misaligned link accepted
 *                     16		   valid setTransfer() rejected
 *                     17 		   count 0 setTransfer() ok
 *                     18 		   startTransfer() after a failed setTransfer() did not refuse
 *                     19 		   busy after a failed setTransfer()
 *                     20 		   count = 1 could not start
 *                     21 		   count = 1024 could not start
 *                     22 		   count = 1: wrong data / wrote past the end
 *                     23 		   count = 1: flags / error
 *                     24 		   count = 1024: wrong data / wrote past the end
 *                     25 		   count = 1024: flags / error
 *
 *      T_WIDTHS     30..34 	   could not start (sub test 0..4)
 *      			 40..44 	   wrong data / wrote past the end (sub test 0..4)
 *                   50..54 	   flags / error (sub test 0..4)
 *
 *                  sub test: 0 = 16-bit INC_1,
 *                  		  1 = 16-bit src INC_2,
 *                  		  2 = 32-bit src INC_4,
 *                  		  3 = dst INC_NONE,
 *                  		  4 = 8-bit dst INC_4
 *
 *      T_LINEAR       60 		   could not start
 *      			   61 		   head: wrong data
 *      			   62  		   head: later descriptors already ran
 *                     63  		   head: isBusy() false (chain pending)
 *                     64  		   head: INTA set too early
 *                     65  		   2nd descriptor: wrong data
 *                     66  		   2nd: last descriptor already ran
 *                     67  		   2nd: isBusy() false (chain pending)
 *                     68  		   2nd: INTA set too early
 *                     69  		   last: wrong data
 *                     70  		   last: INTB set
 *  				   71  		   error flag at the end
 *
 *      T_ABORT        72 		   could not start
 *      			   73  		   still busy after stopDone()
 *                     74  		   INTA set on an aborted transfer
 *                     75  		   wrote past the end
 *                     76  		   something was written AFTER the abort
 *
 *      T_RESTART      77  		   could not start
 *      			   78  		   wrong data / flags / error
 *
 *      T_REUSE      80..83 	   iteration of type 0..3: wrong data / flags / error / could not start
 *                  			   type: 0 = 32-bit,
 *                  			   		 1 = 8-bit fixed source + INTB,
 *                  			   		 2 = 16-bit INTA only,
 *                  			   		 3 = 32-bit dst INC_2
 *
 *      T_PP2SINGLE    90  		   could not start
 *      			   91  		   A: wrong data
 *      			   92  		   A: isBusy() false
 *      			   93  		   B: wrong data
 *      			   94  		   B: isBusy() false
 *                     95  		   busy after stopDone()
 *                     96  		   single transfer could not start
 *                     97  		   single transfer: wrong data / flags / error
 *
 *      Any test       99  		   timeout
 */

namespace{
	const uint16_t BIG = 1024;

	//	Limits / Abort Test
	uint8_t s_oneSrc[2];
	uint8_t s_oneDst[3];
	uint8_t s_bigSrc[BIG];
	uint8_t s_bigDst[BIG + 2];	//	+ 2 sentinel bytes

	//	Widths Test
	uint16_t s_w16src[40];
	uint16_t s_w16dst[22];					//	20 + 2 sentinels
	uint32_t s_w32src[32];
	uint32_t s_w32dst[10];					//	8 + 2 sentinels
	uint32_t s_fix[3];						//	sentinel, target, sentinel
	uint8_t s_b8src[8];
	uint8_t s_b8dst[32];

	//	Linear Chain Test
	uint8_t s_lin8src[8];
	uint8_t s_lin8dst[8];
	uint16_t s_lin16src[8];
	uint16_t s_lin16dst[8];
	uint32_t s_lin32src[4];
	uint32_t s_lin32dst[4];
	Dma::Descriptor s_linD1;				//	16-byte aligned by the type
	Dma::Descriptor s_linD2;

	//	Reuse / Restart / PingPong Test
	uint32_t s_reuSrc[8];
	uint32_t s_reuDst[16];
	uint8_t s_reuByte;
	uint8_t s_reuDst8[8];
	uint16_t s_reuSrc16[8];
	uint16_t s_reuDst16[8];

	uint32_t s_ppSrc[8];
	uint32_t s_ppA[8];
	uint32_t s_ppB[8];
	Dma::Descriptor s_ppDescA;
	Dma::Descriptor s_ppDescB;

	//	Helpers
	uint16_t countNonZero(const uint8_t *buf, uint16_t len){
		uint16_t n = 0;
		for(uint16_t idx = 0; idx < len; idx++)
			if(buf[idx] != 0)	n++;
		return n;
	}

	bool equal32(const uint32_t *a, const uint32_t *b, uint8_t n){
		for(uint8_t idx = 0; idx < n; idx++)
			if(a[idx] != b[idx])	return false;
		return true;
	}

	bool allZero32(const uint32_t *a, uint8_t n){
		for(uint8_t idx = 0; idx < n; idx++)
			if(a[idx] != 0)	return false;
		return true;
	}
}

DmaTest2::DmaTest2(uint8_t channel) : m_dma(channel){
	m_state = state_t::S_IDLE;
	m_stateAfterRecover = state_t::S_FINISHED;
	m_passMask = 0;
	m_failMask = 0;
	m_firstFailId = 0;
	m_abortCopied = 0;
	m_reuseFailIter = 0xFFFF;
	m_mainLoops = 0;
	m_iterationNumber = 0;
	m_subStep = 0;
	m_okAccumFlag = true;
	m_abortOkFlag = true;
}

void DmaTest2::init(void){
	m_passMask = 0;
	m_failMask = 0;
	m_firstFailId = 0;
	m_reuseFailIter = 0xFFFF;
	m_abortOkFlag = true;
	DmaTest2::nextTest(state_t::S_LIMITS_REJECT);
}

bool DmaTest2::hasFinished(void) const{ return (m_state == state_t::S_FINISHED); }

bool DmaTest2::allPassed(void) const{ return ((m_state == state_t::S_FINISHED) && (m_passMask == testBit_t::T_ALL) && (m_failMask == 0)); }

uint8_t DmaTest2::getPassMask(void) const{ return m_passMask; }

uint8_t DmaTest2::getFailMask(void) const{ return m_failMask; }

uint8_t DmaTest2::getFirstFailId(void) const{ return m_firstFailId; }

uint16_t DmaTest2::getAbortCopied(void) const{ return m_abortCopied; }

uint16_t DmaTest2::getReuseFailIteration(void) const{ return m_reuseFailIter; }

void DmaTest2::report(testBit_t bit, bool ok){
	if(ok)	m_passMask |= bit;
	else	m_failMask |= bit;
}

void DmaTest2::check(uint8_t id, bool cond){
	if(!cond){
		m_okAccumFlag = false;
		if(m_firstFailId == 0)	m_firstFailId = id;
	}
}

void DmaTest2::nextTest(state_t nextState){
	m_subStep = 0;
	m_iterationNumber = 0;
	m_mainLoops = 0;
	m_okAccumFlag = true;
	m_state = nextState;
}

void DmaTest2::finishTest(testBit_t bit, state_t nextState){
	DmaTest2::report(bit, m_okAccumFlag);
	DmaTest2::nextTest(nextState);
}

void DmaTest2::timeoutRecover(testBit_t bit, state_t nextState){
	DmaTest2::report(bit, false);
	if(m_firstFailId == 0)	m_firstFailId = 99;
	m_dma.requestStop();
	m_stateAfterRecover = nextState;
	m_mainLoops = 0;
	m_state = state_t::S_RECOVER;
}

bool DmaTest2::fireTest(const void *src, void *dst, uint16_t count, Dma::wordWidth_t width, Dma::addrInc_t srcInc, Dma::addrInc_t dstInc, uint8_t options, const Dma::Descriptor *next){
	m_mainLoops = 0;
	return (m_dma.setTransfer(src, dst, count, width, srcInc, dstInc, options, next) && m_dma.startTransfer());
}


void DmaTest2::stateMachine(void){
	switch(m_state){

		case state_t::S_IDLE:
		case state_t::S_FINISHED:
			break;


		// --------------------------------------- T_LIMITS --------------------------------------- //
		case state_t::S_LIMITS_REJECT:{
			Dma::Descriptor d;

			DmaTest2::check(11, !Dma::buildDescriptor(d, s_oneSrc, s_oneDst, 0, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1));
			DmaTest2::check(12, !Dma::buildDescriptor(d, s_bigSrc, s_bigDst, (BIG + 1), Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1));
			DmaTest2::check(13, !Dma::buildDescriptor(d, (((const uint8_t*)s_w32src) + 1), s_w32dst, 4, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1));
			DmaTest2::check(14, !Dma::buildDescriptor(d, s_w16src, (((uint8_t*)s_w16dst) + 1), 4, Dma::WIDTH_16BIT, Dma::INC_1, Dma::INC_1));
			DmaTest2::check(15, !Dma::buildDescriptor(d, s_oneSrc, s_oneDst, 1, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1,
											Dma::OPT_INTA, (const Dma::Descriptor*)(((const uint8_t*)&s_linD1) + 4)));

			//	A Failed setTransfer() Must Invalidate the Previous One: startTransfer() HAS to Refuse
			DmaTest2::check(16, m_dma.setTransfer(s_oneSrc, s_oneDst, 1, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1));
			DmaTest2::check(17, !m_dma.setTransfer(s_oneSrc, s_oneDst, 0, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1));
			DmaTest2::check(18, !m_dma.startTransfer());	//	This one Should Refuse
			DmaTest2::check(19, !m_dma.isBusy());

			m_subStep = 0;
			m_state = state_t::S_LIMITS_START;
			break;
		}

		case state_t::S_LIMITS_START:{
			bool f_ok;
			if(m_subStep == 0){	//	count = 1: Only ONE Byte May Be Written
				s_oneSrc[0] = 0x77;
				s_oneSrc[1] = 0x88;
				s_oneDst[0] = 0x00;
				s_oneDst[1] = 0xEE;
				s_oneDst[2] = 0xEE;
				f_ok = DmaTest2::fireTest(s_oneSrc, s_oneDst, 1, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1);
			}else{			//	count = 1024 (Maximum, XFERCOUNT = 1023)
				for(uint16_t idx = 0; idx < BIG; idx++){
					s_bigSrc[idx] = (uint8_t)((idx % 251) + 1);	//	Never 0
					s_bigDst[idx] = 0;
				}
				s_bigDst[BIG] = 0xEE;	//	Sentinels
				s_bigDst[BIG + 1] = 0xEE;
				f_ok = DmaTest2::fireTest(s_bigSrc, s_bigDst, BIG, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1);
			}

			if(f_ok){
				m_state = state_t::S_LIMITS_WAIT;
			}else{
				DmaTest2::check((20 + m_subStep), false);
				if(m_subStep == 0)	m_subStep = 1;
				else	DmaTest2::finishTest(testBit_t::T_LIMITS, state_t::S_WIDTHS_START);
			}
			break;
		}

		case state_t::S_LIMITS_WAIT:
			if(!m_dma.isBusy()){
				if(m_subStep == 0){
					DmaTest2::check(22, ((s_oneDst[0] == 0x77) && (s_oneDst[1] == 0xEE) && (s_oneDst[2] == 0xEE)));
					DmaTest2::check(23, (m_dma.aIsDone() && !m_dma.hasError()));
				}else{
					bool f_ok = ((s_bigDst[BIG] == 0xEE) && (s_bigDst[BIG + 1] == 0xEE));
					for(uint16_t idx = 0; idx < BIG; idx++)
						if(s_bigDst[idx] != s_bigSrc[idx])	f_ok = false;
					DmaTest2::check(24, f_ok);
					DmaTest2::check(25, (m_dma.aIsDone() && !m_dma.hasError()));
				}

				if(m_subStep == 0){
					m_subStep = 1;
					m_state = state_t::S_LIMITS_START;
				}else{
					DmaTest2::finishTest(testBit_t::T_LIMITS, state_t::S_WIDTHS_START);
				}
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest2::timeoutRecover(testBit_t::T_LIMITS, state_t::S_WIDTHS_START);
			}
			break;

		// --------------------------------------- T_WIDTHS --------------------------------------- //
		case state_t::S_WIDTHS_START:{
			bool f_ok = false;

			switch(m_subStep){
				case 0:	//	16-bit, Both Addresses +1 Item
					for(uint8_t idx = 0; idx < 20; idx++)	s_w16src[idx] = (uint16_t)(0x1200 + idx);
					for(uint8_t idx = 0; idx < 20; idx++)	s_w16dst[idx] = 0;
					s_w16dst[20] = 0xEEEE;
					s_w16dst[21] = 0xEEEE;
					f_ok = DmaTest2::fireTest(s_w16src, s_w16dst, 20, Dma::WIDTH_16BIT, Dma::INC_1, Dma::INC_1);
					break;

				case 1:	//	16-bit, Source +2 Items, Destination +1 Item: dst[k] = src[2k]
					for(uint8_t idx = 0; idx < 40; idx++)	s_w16src[idx] = (uint16_t)(0x3400 + idx);
					for(uint8_t idx = 0; idx < 20; idx++)	s_w16dst[idx] = 0;
					s_w16dst[20] = 0xEEEE;
					s_w16dst[21] = 0xEEEE;
					f_ok = DmaTest2::fireTest(s_w16src, s_w16dst, 20, Dma::WIDTH_16BIT, Dma::INC_2, Dma::INC_1);
					break;

				case 2:	//	32-bit, Source +4 Items (Register Field 3), Destination +1 Item: dst[k] = src[4k]
					for(uint8_t idx = 0; idx < 32; idx++)	s_w32src[idx] = (0x56780000u + idx);
					for(uint8_t idx = 0; idx < 8; idx++)	s_w32dst[idx] = 0;
					s_w32dst[8] = 0xDEADBEEFu;
					s_w32dst[9] = 0xDEADBEEFu;
					f_ok = DmaTest2::fireTest(s_w32src, s_w32dst, 8, Dma::WIDTH_32BIT, Dma::INC_4, Dma::INC_1);
					break;

				case 3:	//	32-bit, Fixed Destination: Only the Last Item Should Survive
					for(uint8_t idx = 0; idx < 8; idx++)	s_w32src[idx] = (0x9A000000u + idx);
					s_fix[0] = 0xDEADBEEFu;
					s_fix[1] = 0;
					s_fix[2] = 0xDEADBEEFu;
					f_ok = DmaTest2::fireTest(s_w32src, &s_fix[1], 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_NONE);
					break;

				default:	//	8-bit, Destination +4 Items: dst[4k] = src[k], the Rest Untouched
					for(uint8_t idx = 0; idx < 8; idx++)	s_b8src[idx] = (uint8_t)(0x10 + idx);
					for(uint8_t idx = 0; idx < 32; idx++)	s_b8dst[idx] = 0xEE;
					f_ok = DmaTest2::fireTest(s_b8src, s_b8dst, 8, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_4);
					break;
			}

			if(f_ok){
				m_state = state_t::S_WIDTHS_WAIT;
			}else{
				DmaTest2::check((30 + m_subStep), false);
				if(m_subStep < 4)	m_subStep++;
				else	DmaTest2::finishTest(testBit_t::T_WIDTHS,state_t:: S_LINEAR_START);
			}
			break;
		}

		case state_t::S_WIDTHS_WAIT:
			if(!m_dma.isBusy()){
				bool f_ok = true;

				switch(m_subStep){
					case 0:
						for(uint8_t idx = 0; idx < 20; idx++)	if(s_w16dst[idx] != (0x1200 + idx))	f_ok = false;
						if((s_w16dst[20] != 0xEEEE) || (s_w16dst[21] != 0xEEEE))	f_ok = false;
						break;
					case 1:
						for(uint8_t idx = 0; idx < 20; idx++)	if(s_w16dst[idx] != (0x3400 + (2 * idx)))	f_ok = false;
						if((s_w16dst[20] != 0xEEEE) || (s_w16dst[21] != 0xEEEE))	f_ok = false;
						break;
					case 2:
						for(uint8_t idx = 0; idx < 8; idx++)	if(s_w32dst[idx] != (0x56780000u + (4 * idx)))	f_ok = false;
						if((s_w32dst[8] != 0xDEADBEEFu) || (s_w32dst[9] != 0xDEADBEEFu))	f_ok = false;
						break;
					case 3:
						if(s_fix[1] != (0x9A000000u + 7))	f_ok = false;
						if((s_fix[0] != 0xDEADBEEFu) || (s_fix[2] != 0xDEADBEEFu))	f_ok = false;
						break;
					default:
						for(uint8_t idx = 0; idx < 32; idx++){
							uint8_t expected = (((idx % 4) == 0) ? (uint8_t)(0x10 + (idx / 4)) : 0xEE);
							if(s_b8dst[idx] != expected)	f_ok = false;
						}
						break;
				}

				DmaTest2::check((40 + m_subStep), f_ok);
				DmaTest2::check((50 + m_subStep), (m_dma.aIsDone() && !m_dma.hasError()));

				if(m_subStep < 4){
					m_subStep++;
					m_state = state_t::S_WIDTHS_START;
				}else{
					DmaTest2::finishTest(testBit_t::T_WIDTHS, state_t::S_LINEAR_START);
				}
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest2::timeoutRecover(testBit_t::T_WIDTHS, state_t::S_LINEAR_START);
			}
			break;

		// --------------------------------------- T_LINEAR --------------------------------------- //
		case state_t::S_LINEAR_START:{
			for(uint8_t idx = 0; idx < 8; idx++){
				s_lin8src[idx] = (uint8_t)(0x20 + idx);
				s_lin8dst[idx] = 0;
				s_lin16src[idx] = (uint16_t)(0x4000 + idx);
				s_lin16dst[idx] = 0;
			}
			for(uint8_t idx = 0; idx < 4; idx++){
				s_lin32src[idx] = (0x70000000u + idx);
				s_lin32dst[idx] = 0;
			}

			//	Head (Channel Table, 8-bit) -> s_linD1 (16-bit) -> s_linD2 (32-bit, Last, NO Reload).
			//	CLRTRIG in All of Them: Every Descriptor Needs its Own setTrig(). INTB Marks the First Two, INTA the Last One.
			bool f_ok = (Dma::buildDescriptor(s_linD2, s_lin32src, s_lin32dst, 4, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTA | Dma::OPT_CLRTRIG), nullptr)
					  && Dma::buildDescriptor(s_linD1, s_lin16src, s_lin16dst, 8, Dma::WIDTH_16BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTB | Dma::OPT_CLRTRIG), &s_linD2)
					  && DmaTest2::fireTest(s_lin8src, s_lin8dst, 8, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTB | Dma::OPT_CLRTRIG), &s_linD1));

			if(f_ok){
				m_subStep = 0;
				m_state = state_t::S_LINEAR_WAIT;
			}else{
				DmaTest2::check(60, false);
				DmaTest2::finishTest(testBit_t::T_LINEAR, state_t::S_ABORT_START);
			}
			break;
		}

		case state_t::S_LINEAR_WAIT:
			switch(m_subStep){
				case 0:	//	Head Done (INTB)
					if(m_dma.bIsDone()){
						bool f_ok = true;
						for(uint8_t idx = 0; idx < 8; idx++)	if(s_lin8dst[idx] != s_lin8src[idx])	f_ok = false;
						DmaTest2::check(61, f_ok);
						f_ok = true;
						for(uint8_t idx = 0; idx < 8; idx++)	if(s_lin16dst[idx] != 0)	f_ok = false;
						DmaTest2::check(62, (f_ok && allZero32(s_lin32dst, 4)));
						DmaTest2::check(63, m_dma.isBusy());		//	Two Descriptors Still Pending
						DmaTest2::check(64, !m_dma.aIsDone());
						m_dma.clearBflags();
						m_dma.setTrig();
						m_subStep = 1;
						m_mainLoops = 0;
					}
					break;

				case 1:	//	Second Descriptor Done (INTB)
					if(m_dma.bIsDone()){
						bool f_ok = true;
						for(uint8_t idx = 0; idx < 8; idx++)	if(s_lin16dst[idx] != s_lin16src[idx])	f_ok = false;
						DmaTest2::check(65, f_ok);
						DmaTest2::check(66, allZero32(s_lin32dst, 4));
						DmaTest2::check(67, m_dma.isBusy());		//	Last Descriptor Still Pending
						DmaTest2::check(68, !m_dma.aIsDone());
						m_dma.clearBflags();
						m_dma.setTrig();
						m_subStep = 2;
						m_mainLoops = 0;
					}
					break;

				case 2:	//	Last Descriptor Done (INTA)
					if(m_dma.aIsDone()){
						DmaTest2::check(69, equal32(s_lin32dst, s_lin32src, 4));
						DmaTest2::check(70, !m_dma.bIsDone());
						m_subStep = 3;
						m_mainLoops = 0;
					}
					break;

				default:	//	No Reload Left: the Channel HAS to Stop Being Busy
					if(!m_dma.isBusy()){
						DmaTest2::check(71, !m_dma.hasError());
						DmaTest2::finishTest(testBit_t::T_LINEAR, state_t::S_ABORT_START);
					}
					break;
			}

			if((m_state == state_t::S_LINEAR_WAIT) && (++m_mainLoops > TIMEOUT_LOOPS))
				DmaTest2::timeoutRecover(testBit_t::T_LINEAR, state_t::S_ABORT_START);
			break;

		// --------------------------------------- T_ABORT --------------------------------------- //
		case state_t::S_ABORT_START:
			for(uint16_t idx = 0; idx < BIG; idx++){
				s_bigSrc[idx] = (uint8_t)((idx % 251) + 1);
				s_bigDst[idx] = 0;
			}
			s_bigDst[BIG] = 0xEE;
			s_bigDst[BIG + 1] = 0xEE;
			m_abortOkFlag = true;

			if(DmaTest2::fireTest(s_bigSrc, s_bigDst, BIG, Dma::WIDTH_8BIT, Dma::INC_1, Dma::INC_1)){
				m_dma.requestStop();	//	Right After the Start: the Transfer is (Probably?) Still in Progress
				m_mainLoops = 0;
				m_state = state_t::S_ABORT_STOPPING;
			}else{
				if(m_firstFailId == 0)	m_firstFailId = 72;
				DmaTest2::report(testBit_t::T_ABORT, false);
				DmaTest2::nextTest(state_t::S_RESTART_START);
			}
			break;

		case state_t::S_ABORT_STOPPING:
			if(m_dma.stopDone()){
				m_abortCopied = countNonZero(s_bigDst, BIG);	//	1024 = the Abort Came Too Late (Still Valid, Ideally it Should be 0)

				if(m_dma.isBusy()){
					m_abortOkFlag = false;
					if(m_firstFailId == 0)	m_firstFailId = 73;
				}
				if((m_abortCopied < BIG) && m_dma.aIsDone()){
					m_abortOkFlag = false;
					if(m_firstFailId == 0)	m_firstFailId = 74;
				}
				if((s_bigDst[BIG] != 0xEE) || (s_bigDst[BIG + 1] != 0xEE)){
					m_abortOkFlag = false;
					if(m_firstFailId == 0)	m_firstFailId = 75;
				}
				DmaTest2::nextTest(state_t::S_RESTART_START);
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest2::report(testBit_t::T_ABORT, false);
				DmaTest2::report(testBit_t::T_RESTART, false);
				if(m_firstFailId == 0)	m_firstFailId = 99;
				DmaTest2::nextTest(state_t::S_REUSE_START);
			}
			break;

		// --------------------------------------- T_RESTART --------------------------------------- //
		case state_t::S_RESTART_START:
			for(uint8_t idx = 0; idx < 8; idx++){
				s_reuSrc[idx] = (0xCAFE0000u + idx);
				s_reuDst[idx] = 0;
			}
			if(DmaTest2::fireTest(s_reuSrc, s_reuDst, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1)){
				m_state = state_t::S_RESTART_WAIT;
			}else{
				DmaTest2::check(77, false);
				DmaTest2::report(testBit_t::T_ABORT, m_abortOkFlag);
				DmaTest2::finishTest(testBit_t::T_RESTART, state_t::S_REUSE_START);
			}
			break;

		case state_t::S_RESTART_WAIT:
			if(!m_dma.isBusy()){
				DmaTest2::check(78, (equal32(s_reuDst, s_reuSrc, 8) && m_dma.aIsDone() && !m_dma.hasError()));

				//	Nothing May Have Been Written to the Aborted Destination Since the Abort
				if(countNonZero(s_bigDst, BIG) != m_abortCopied){
					m_abortOkFlag = false;
					if(m_firstFailId == 0)	m_firstFailId = 76;
				}
				DmaTest2::report(testBit_t::T_ABORT, m_abortOkFlag);
				DmaTest2::finishTest(testBit_t::T_RESTART, state_t::S_REUSE_START);
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				DmaTest2::report(testBit_t::T_ABORT, m_abortOkFlag);
				DmaTest2::timeoutRecover(testBit_t::T_RESTART, state_t::S_REUSE_START);
			}
			break;

		// --------------------------------------- T_REUSE --------------------------------------- //
		case state_t::S_REUSE_START:{
			bool f_ok = false;

			switch(m_iterationNumber & 3){
				case 0:	//	32-bit, INTA | CLRTRIG
					for(uint8_t idx = 0; idx < 8; idx++){
						s_reuSrc[idx] = (0xAB000000u + ((uint32_t)m_iterationNumber << 8) + idx);
						s_reuDst[idx] = 0;
					}
					f_ok = DmaTest2::fireTest(s_reuSrc, s_reuDst, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1);
					break;

				case 1:	//	8-bit, fixed source, INTA | INTB | CLRTRIG
					s_reuByte = (uint8_t)(m_iterationNumber | 1);
					for(uint8_t idx = 0; idx < 8; idx++)	s_reuDst8[idx] = 0;
					f_ok = DmaTest2::fireTest(&s_reuByte, s_reuDst8, 8, Dma::WIDTH_8BIT, Dma::INC_NONE, Dma::INC_1, (Dma::OPT_INTA | Dma::OPT_INTB | Dma::OPT_CLRTRIG));
					break;

				case 2:	//	16-bit, INTA Only (No Stale INTB From the Previous Iteration)
					for(uint8_t idx = 0; idx < 8; idx++){
						s_reuSrc16[idx] = (uint16_t)(0x5000 + m_iterationNumber + idx);
						s_reuDst16[idx] = 0;
					}
					f_ok = DmaTest2::fireTest(s_reuSrc16, s_reuDst16, 8, Dma::WIDTH_16BIT, Dma::INC_1, Dma::INC_1, Dma::OPT_INTA);
					break;

				default:	//	32-bit, Destination +2 Items: dst[2k] = src[k]
					for(uint8_t idx = 0; idx < 8; idx++)	s_reuSrc[idx] = (0xCD000000u + ((uint32_t)m_iterationNumber << 8) + idx);
					for(uint8_t idx = 0; idx < 16; idx++)	s_reuDst[idx] = 0;
					f_ok = DmaTest2::fireTest(s_reuSrc, s_reuDst, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_2);
					break;
			}

			if(f_ok){
				m_state = state_t::S_REUSE_WAIT;
			}else{
				DmaTest2::check((80 + (m_iterationNumber & 3)), false);
				if(m_reuseFailIter == 0xFFFF)	m_reuseFailIter = m_iterationNumber;
				m_iterationNumber = DmaTest2::REUSE_ITERATIONS;	//	Nothing Else Makes Sense
				DmaTest2::finishTest(testBit_t::T_REUSE, state_t::S_PP_START);
			}
			break;
		}

		case state_t::S_REUSE_WAIT:
			if(!m_dma.isBusy()){
				bool f_ok = (m_dma.aIsDone() && !m_dma.hasError());

				switch(m_iterationNumber & 3){
					case 0:
						f_ok = (f_ok && equal32(s_reuDst, s_reuSrc, 8) && !m_dma.bIsDone());
						break;
					case 1:
						for(uint8_t idx = 0; idx < 8; idx++)	if(s_reuDst8[idx] != (uint8_t)(m_iterationNumber | 1))	f_ok = false;
						f_ok = (f_ok && m_dma.bIsDone());
						break;
					case 2:
						for(uint8_t idx = 0; idx < 8; idx++)	if(s_reuDst16[idx] != (uint16_t)(0x5000 + m_iterationNumber + idx))	f_ok = false;
						f_ok = (f_ok && !m_dma.bIsDone());	//	A Stale SETINTB in XFERCFG Would Show Up Here
						break;
					default:
						for(uint8_t idx = 0; idx < 16; idx++){
							uint32_t expected = (((idx % 2) == 0) ? (0xCD000000u + ((uint32_t)m_iterationNumber << 8) + (idx / 2)) : 0);
							if(s_reuDst[idx] != expected)	f_ok = false;
						}
						f_ok = (f_ok && !m_dma.bIsDone());
						break;
				}

				DmaTest2::check((80 + (m_iterationNumber & 3)), f_ok);
				if(!f_ok && (m_reuseFailIter == 0xFFFF))	m_reuseFailIter = m_iterationNumber;

				if(++m_iterationNumber >= DmaTest2::REUSE_ITERATIONS)	DmaTest2::finishTest(testBit_t::T_REUSE, state_t::S_PP_START);
				else	m_state = state_t::S_REUSE_START;
			}else if(++m_iterationNumber > TIMEOUT_LOOPS){
				if(m_reuseFailIter == 0xFFFF)	m_reuseFailIter = m_iterationNumber;
				DmaTest2::timeoutRecover(testBit_t::T_REUSE, state_t::S_PP_START);
			}
			break;

		// --------------------------------------- T_PP2SINGLE --------------------------------------- //
		case state_t::S_PP_START:{
			for(uint8_t idx = 0; idx < 8; idx++){
				s_ppSrc[idx] = (0x7E570000u + idx);
				s_ppA[idx] = 0;
				s_ppB[idx] = 0;
			}

			//	Table (A) -> s_ppDescB (B) -> s_ppDescA (A) -> ...   One setTrig() per Buffer
			bool f_ok = (Dma::buildDescriptor(s_ppDescA, s_ppSrc, s_ppA, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTA | Dma::OPT_CLRTRIG), &s_ppDescB)
					  && Dma::buildDescriptor(s_ppDescB, s_ppSrc, s_ppB, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTB | Dma::OPT_CLRTRIG), &s_ppDescA)
				      && DmaTest2::fireTest(s_ppSrc, s_ppA, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1, (Dma::OPT_INTA | Dma::OPT_CLRTRIG), &s_ppDescB));

			if(f_ok){
				m_subStep = 0;
				m_state = state_t::S_PP_WAIT;
			}else{
				DmaTest2::check(90, false);
				DmaTest2::finishTest(testBit_t::T_PP2SINGLE, state_t::S_FINISHED);
			}
			break;
		}

		case state_t::S_PP_WAIT:
			if(m_subStep == 0){
				if(m_dma.aIsDone()){
					DmaTest2::check(91, equal32(s_ppA, s_ppSrc, 8));
					DmaTest2::check(92, m_dma.isBusy());
					m_dma.clearAflags();
					m_dma.setTrig();
					m_subStep = 1;
					m_mainLoops = 0;
				}
			}else{
				if(m_dma.bIsDone()){
					DmaTest2::check(93, equal32(s_ppB, s_ppSrc, 8));
					DmaTest2::check(94, m_dma.isBusy());
					m_dma.clearBflags();
					m_dma.requestStop();
					m_mainLoops = 0;
					m_state = state_t::S_PP_STOPPING;
				}
			}

			if((m_state == state_t::S_PP_WAIT) && (++m_mainLoops > TIMEOUT_LOOPS))
				DmaTest2::timeoutRecover(testBit_t::T_PP2SINGLE, state_t::S_FINISHED);
			break;

		case state_t::S_PP_STOPPING:
			if(m_dma.stopDone()){
				DmaTest2::check(95, !m_dma.isBusy());
				m_state = state_t::S_SINGLE_START;
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				if(m_firstFailId == 0)	m_firstFailId = 99;
				DmaTest2::report(testBit_t::T_PP2SINGLE, false);
				DmaTest2::nextTest(state_t::S_FINISHED);
			}
			break;

		case state_t::S_SINGLE_START:
			for(uint8_t idx = 0; idx < 8; idx++){
				s_reuSrc[idx] = (0x51000000u + idx);
				s_reuDst[idx] = 0;
			}
			//	Same Channel, now WITHOUT [next]: isBusy() HAS to End on its Own Again
			if(DmaTest2::fireTest(s_reuSrc, s_reuDst, 8, Dma::WIDTH_32BIT, Dma::INC_1, Dma::INC_1)){
				m_state = state_t::S_SINGLE_WAIT;
			}else{
				DmaTest2::check(96, false);
				DmaTest2::finishTest(testBit_t::T_PP2SINGLE, state_t::S_FINISHED);
			}
			break;

		case state_t::S_SINGLE_WAIT:
			if(!m_dma.isBusy()){
				DmaTest2::check(97, (equal32(s_reuDst, s_reuSrc, 8) && m_dma.aIsDone() && !m_dma.bIsDone() && !m_dma.hasError()));
				DmaTest2::finishTest(testBit_t::T_PP2SINGLE, state_t::S_FINISHED);
			}else if(++m_mainLoops > TIMEOUT_LOOPS){
				if(m_firstFailId == 0)	m_firstFailId = 99;
				DmaTest2::report(testBit_t::T_PP2SINGLE, false);
				DmaTest2::nextTest(state_t::S_FINISHED);
			}
			break;

		// --------------------------------------- T_TIMEOUT_RECOVERY --------------------------------------- //
		case state_t::S_RECOVER:
			if(m_dma.stopDone() || (++m_mainLoops > TIMEOUT_LOOPS)){
				m_dma.clearFlags();
				DmaTest2::nextTest(m_stateAfterRecover);
			}
			break;

		default:
			m_state = state_t::S_FINISHED;
			break;
	}
}
