/*
 * my_adc.cpp
 *
 *  Created on: 23 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "adc.h"
/*

Adc *adcInstance[Adc::TOTAL_ADC_CHANNELS] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

uint32_t Adc::s_seqBase[2] = {0, 0};
bool Adc::s_seqConfigured[2] = {false, false};
uint16_t Adc::s_seqMask[2] = {0, 0};
uint8_t Adc::s_adcChannelsNeeded = 0;

//	All Port 0
static const uint8_t adcPin[Adc::TOTAL_ADC_CHANNELS] = {7, 6, 14, 23, 22, 21, 20, 19, 18, 17, 13, 4};

//	NVIC (Cap. 7), SYSCON (Cap. 8), SWM (Cap. 10), IOCON (Cap. 11), GPIO (Cap. 12), DMA (Cap. 16), ADC (Cap. 27)
Adc::Adc(adcChannel_t adcChannel, uint32_t freqHz, adcTrigSrc_t trigSrc, adcTrigPol_t trigPol, bool burst, bool singleStep, adcSeq_t adcSeq, adcAsyncMode_t asyncMode, adcPwrMode_t pwrMode, bool enableBypassSync, bool lowPriority, adcRetrievingConversionMethod_t modeBit){
	m_adcChannel = adcChannel;
	m_freqHz = freqHz;
	m_adcSequence = adcSeq;
	m_adcTrigSrc = trigSrc;
	m_adcTrigPol = trigPol;
	m_burstFlag = burst;
	m_singleStepFlag = singleStep;
	m_adcAsyncMode = asyncMode;
	m_adcPwrMode = pwrMode;
	m_adcBypassSyncFlag = enableBypassSync;
	m_adcLowPriorityFlag = lowPriority;
	m_adcMode = modeBit;

	m_adcInitFinishedFlag = false;
	m_adcStartedFlag = false;

	m_adcLastSample = 0;
	m_adcSamplesBuffIndex = 0;
	m_adcValidSamplesCounter = 0;
	m_adcNewSampleFlag = false;
	for(uint8_t idx = 0; idx < Adc::TOTAL_ADC_SAMPLES; idx++)	m_adcSamplesBuff[idx] = 0;

	m_adcLastRange = 0;
	m_adcLastCross = 0;
	m_adcThFlag = false;
	m_adcThRange = 0;
	m_adcThCross = 0;
	m_adcThCount = 0;

	m_adcOvrFlag = false;
	m_adcOvrCounter = 0;
}

bool Adc::init(bool launch){
	if(m_adcInitFinishedFlag)	return true;
	if(adcInstance[m_adcChannel] != nullptr)	return false;	//	Channel Taken

	//	SEQx_CTRL Bits Shared by Every Channel in the Sequence
	uint32_t base = ((m_adcTrigSrc << 12)			//	Selects HW Trigger Source
				   | (m_adcTrigPol << 18)			//	Selects Input Trigger Polarity
				   | (m_adcBypassSyncFlag << 19)	//	Check Table 454 from Usr Manual
				   | (m_burstFlag << 27)			//	1: Continuously Cycled Through. Other SEQx Triggers Will be Ignored while this Bit is Set
				   | (m_singleStepFlag << 28)		//	1: Single Conversion on the Next Channel in the Sequence Instead of the Default Response of Launching an Entire Sequence of Conversions. Once All Channels Comprising a Sequence Have Been Converted, a Subsequent Trigger Will Repeat the Sequence Beginning With the First Enabled Channel
				   | (m_adcMode << 30));			//	0: SEQx Interrupt/DMA Flag Will Be Set at the End of Each Individual Conversion. This Flag Will Mirror DATAVALID Bit in SEQx_GDAT Register. OVERRUN Bit in SEQx_GDAT Register Will Contribute to Generation of an Overrun Interrupt if Enabled. - 1: SEQx Interrupt/DMA Flag Will Be Set When Entire Sequence Conversions Completes. This Flag Will Need to be Cleared by Software or DMA-clear Signal. OVERRUN Bit in SEQx_GDAT Register Will NOT Generate an Overrun Interrupt.

	if(s_seqConfigured[m_adcSequence] && (s_seqBase[m_adcSequence] != base))	return false;	//	Previously Configured Different
	if(s_adcChannelsNeeded && ((s_adcAsync != m_adcAsyncMode) || (s_adcPwr != m_adcPwrMode)))	return false;	//	Settings Differ

	if(s_adcChannelsNeeded == 0){
		s_adcAsync = m_adcAsyncMode;
		s_adcPwr = m_adcPwrMode;
	}

	s_seqBase[m_adcSequence] = base;
	s_seqConfigured[m_adcSequence] = true;

	//	This Bit Is Only Managed in SEQA_CTRL (Handles Both SEQA and SEQB)
	s_lowPriorityBit = ((m_adcSequence == adcSeq_t::SEQ_A) == m_adcLowPriorityFlag) ? (1 << 29) : 0;

	adcInstance[m_adcChannel] = this;
	s_adcChannelsNeeded++;

	if(m_freqHz > Adc::MAX_ADC_FREQ)	m_freqHz = Adc::MAX_ADC_FREQ;

	if(m_adcSequence == adcSeq_t::SEQ_B)	commitSeq(0);

	//	Start HW
	SYSCON->PDRUNCFG &= ~(1 << 4);						//	ADC_PD: Powered
	SYSCON->SYSAHBCLKCTRL0 |= ((1 << 7) | (1 << 24));	//	Enable SWM and ADC Clk
	SYSCON->ADCCLKSEL &= ~0x3;	//	ADC Clk Source: FRO
	SYSCON->ADCCLKDIV &= ~0xFF;	//	ADC Clk Div: Disabled
	SYSCON->ADCCLKDIV |= 0x1;	//	ADC Clk Div: 1

	disableIoconPullResistors();	//	The MODE bits in the IOCON register should also disable both pull-up and pull-down resistors

	SWM->PINENABLE0 &= ~(1 << (14 + m_adcChannel));	//	Enable ADCx

	//	Calibration (should be in a single write to the ADC CTRL register)
	uint8_t clkDiv = ((FREQ_CLOCK / 500000) - 1);			//	"The system clock is divided by this value plus one"
	ADC->CTRL = ((clkDiv << 0) | (0 << 10) | (1 << 30));	//	CLKDIV Should Set Freq in 500kHz, LPWR = 0, CALMODE = 1
	while((ADC->CTRL & (1 << 30)));		//	Polling (~290us), Waits Calibration to Finish

	//	Each A/D Conversion Takes 25 ADC Clks
	//	The ADC Clk is Shared Among All ADC Being Used (AT SAME TIME)
	//	So: samplingFreq = (clkFreq / (25 * freqWanted * sourcesNeeded)) - 1
	uint32_t samplingClk = (25 * m_freqHz * adcChannelsNeeded);
	clkDiv = (FREQ_CLOCK / samplingClk);
	clkDiv = clkDiv ? (clkDiv - 1) : clkDiv;	//	If clkFreq / samplingClk = 0, Do Not Subtract

	ADC->CTRL = ((clkDiv << 0)		//	The system clock is divided by this value plus one
			   | (asyncMode << 8)	//	0: ADC Clk Derived from mainClk Based on CLKDIV - 1: ADC Clk Based on an Alternative Independent Clk Source (15 MHz Max, Slower than 10 * APB Bus Clk Rate)
			   | (pwrMode << 10)	//	LPWRMODE - 0: Disabled - 1: Enabled
			   | (0 << 30));		//	CALMODE - 0: Disabled - 1: Enabled

	//	SEQ_CTRL
	//	"Remark: Set the BURST and SEQU_ENA bits at the same time"
	ADC->SEQ_CTRL[m_adcSequence] &= ~(1 << 31);				//	SEQx Disabled (Safet To Modify SEQx_CTRL)

		if(m_adcSequence == adcSeq_t::SEQ_A)
			ADC->SEQ_CTRL[0] |= (1 << 29);	//	1: Any Enabled B Sequence Trigger Will Interrupt Sequence A and Launch Sequence B, the Interrupted A Sequence Will Resume After B Sequence Completes
		else
			ADC->SEQ_CTRL[0] &= ~(1 << 29);	//	0: Any B Trigger Which Occurs While an A Conversion Sequence is Active Will Be Ignored and Lost
	}else{
		if(m_adcSequence == adcSeq_t::SEQ_A)
			ADC->SEQ_CTRL[0] &= ~(1 << 29);	//	0: Any B Trigger Which Occurs While an A Conversion Sequence is Active Will Be Ignored and Lost
		else
			ADC->SEQ_CTRL[0] |= (1 << 29);	//	1: Any Enabled B Sequence Trigger Will Interrupt Sequence A and Launch Sequence B, the Interrupted A Sequence Will Resume After B Sequence Completes
	}


	ADC->SEQ_CTRL[m_adcSequence] |= ((1 << m_adcChannel)		//	Selects Channel To Be Sampled
								   | (trigSrc << 12)			//	Selects HW Trigger Source
								   | (trigPol << 18)			//	Selects Input Trigger Polarity
								   | (enableBypassSync << 19)	//	Check Table 454 from Usr Manual
								   | (burst << 27)				//	1: Continuously Cycled Through. Other SEQx Triggers Will be Ignored while this Bit is Set
								   | (singleStep << 28)			//	1: Single Conversion on the Next Channel in the Sequence Instead of the Default Response of Launching an Entire Sequence of Conversions. Once All Channels Comprising a Sequence Have Been Converted, a Subsequent Trigger Will Repeat the Sequence Beginning With the First Enabled Channel
								   | (modeBit << 30)			//	0: SEQx Interrupt/DMA Flag Will Be Set at the End of Each Individual Conversion. This Flag Will Mirror DATAVALID Bit in SEQx_GDAT Register. OVERRUN Bit in SEQx_GDAT Register Will Contribute to Generation of an Overrun Interrupt if Enabled. - 1: SEQx Interrupt/DMA Flag Will Be Set When Entire Sequence Conversions Completes. This Flag Will Need to be Cleared by Software or DMA-clear Signal. OVERRUN Bit in SEQx_GDAT Register Will NOT Generate an Overrun Interrupt.
								   | (1 << 31));				//	Enable Sequence [m_adcSequence]

	ADC->TRM &= ~(1 << 5);	//	High Voltage Operating Range (VDD = 2.7V to 3.6V)

	enableInt();		//	Enables SEQx Interrupt and Overrun Interrupt (NOT the Threshold Comparison Interrupt)
	enableNVIC_int();	//	Idem for NVIC
	//	For Enabling Threshold Comparison Interrupt, Set it in Main by Calling enableInt(true, enCmp != DISABLED)
}

void Adc::disableIoconPullResistors(void){
	SYSCON->SYSAHBCLKCTRL0 |= (1 << 18);	//	Enable IOCON clk

	switch(m_adcChannel){
		case adcChannel_t::ADC_0:
			IOCON->PIO[Gpio::iocon_index[0][7]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_1:
			IOCON->PIO[Gpio::iocon_index[0][6]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_2:
			IOCON->PIO[Gpio::iocon_index[0][14]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_3:
			IOCON->PIO[Gpio::iocon_index[0][23]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_4:
			IOCON->PIO[Gpio::iocon_index[0][22]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_5:
			IOCON->PIO[Gpio::iocon_index[0][21]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_6:
			IOCON->PIO[Gpio::iocon_index[0][20]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_7:
			IOCON->PIO[Gpio::iocon_index[0][19]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_8:
			IOCON->PIO[Gpio::iocon_index[0][18]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_9:
			IOCON->PIO[Gpio::iocon_index[0][17]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_10:
			IOCON->PIO[Gpio::iocon_index[0][13]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		case adcChannel_t::ADC_11:
			IOCON->PIO[Gpio::iocon_index[0][4]] &= ~(0x3 << 3);	//	MODE: Inactive
			break;

		default:
			break;
	}

	SYSCON->SYSAHBCLKCTRL0 &= ~(1 << 18);	//	Disables IOCON Clk to Save Power
}

void Adc::enableInt(bool enOvr, adcThresholdComparison_t enCmp){
	uint8_t cmpShift = (3 + (m_adcChannel * 2));
	ADC->INTEN |= ((1 << m_adcSequence) | (enOvr << 2) | (enCmp << cmpShift));
	if(enCmp == adcThresholdComparison_t::DISABLED)
		ADC->INTEN &= ~(0x3 << cmpShift);
}

void Adc::disableInt(bool disOvr, bool disCmp){
	if(disCmp){
		uint8_t cmpShift = (3 + (m_adcChannel * 2));
		ADC->INTEN &= ~((1 << m_adcSequence) | (disOvr << 2) | (0x3 << cmpShift));
	}else{
		ADC->INTEN &= ~((1 << m_adcSequence) | (disOvr << 2));
	}
}

void Adc::enableNVIC_int(bool enOvr, bool enCmp){
	uint8_t seqShift = ADC0_SEQA_IRQn + m_adcSequence;
	NVIC->ISER[0] = ((1 << seqShift) | (enCmp << ADC0_THCMP_IRQn) | (enOvr << ADC0_OVR_IRQn));
}

void Adc::disableNVIC_int(bool disOvr, bool disCmp){
	uint8_t seqShift = ADC0_SEQA_IRQn + m_adcSequence;
	NVIC->ICER[0] = ((1 << seqShift) | (disCmp << ADC0_THCMP_IRQn) | (disOvr << ADC0_OVR_IRQn));
}

bool Adc::triggerSingleSeqConversion(void){
	if(m_burstFlag)	return false;	//	Cant Start Single Sequence Conversion if Burst Was Asked in SEQx_CTRL

	ADC->SEQ_CTRL[m_adcSequence] |= (1 << 26);	//	START Bit (Software Single Trigger)

	return true;
}

Adc::adcChannel_t Adc::getChannel(void){ return m_adcChannel; }

uint32_t Adc::getValue(adcValueFilter_t filter){
	uint32_t ret = 0;

	switch(filter){
		case adcValueFilter_t::F_NONE:
			ret = m_adcLastSample;
			break;

		case adcValueFilter_t::F_MEAN:
			ret = Statistics::mean(m_adcSamplesBuff, Adc::TOTAL_ADC_SAMPLES);
			break;

		case adcValueFilter_t::F_MEDIAN:
			break;

		default:
			//	ERROR
			break;
	}
	return ret;
}

void ADC_SEQA_IRQHandler(void){
	uint32_t SEQA_DAT = ADC->SEQ_GDAT[0];
	Adc::adcChannel_t channel = (Adc::adcChannel_t)((SEQA_DAT >> 26) & 0xF);
	uint16_t sample = ((SEQA_DAT >> 4) & 0xFFF);

	for(uint8_t idx = 0; idx < Adc::TOTAL_ADC_CHANNELS; idx++){
		if(adcInstance[idx] != nullptr){
			if(adcInstance[idx]->getChannel() == channel){
				adcInstance[idx]->m_adcLastSample = sample;
				adcInstance[idx]->m_adcSamplesBuff[adcInstance[idx]->m_adcSamplesBuffIndex++] = sample;
				if(adcInstance[idx]->m_adcSamplesBuffIndex >= Adc::TOTAL_ADC_SAMPLES)	//	Faster than %=
					adcInstance[idx]->m_adcSamplesBuffIndex = 0;
				break;	//	Out of For
			}
		}
	}
}

void ADC_SEQB_IRQHandler(void){
	uint32_t SEQB_DAT = ADC->SEQ_GDAT[1];
	Adc::adcChannel_t channel = (Adc::adcChannel_t)((SEQB_DAT >> 26) & 0xF);
	uint16_t sample = ((SEQB_DAT >> 4) & 0xFFF);

	for(uint8_t idx = 0; idx < Adc::TOTAL_ADC_CHANNELS; idx++){
		if(adcInstance[idx]->getChannel() == channel){
			adcInstance[idx]->m_adcLastSample = sample;
			adcInstance[idx]->m_adcSamplesBuff[adcInstance[idx]->m_adcSamplesBuffIndex++] = sample;
			if(adcInstance[idx]->m_adcSamplesBuffIndex >= Adc::TOTAL_ADC_SAMPLES)	//	Faster than %=
				adcInstance[idx]->m_adcSamplesBuffIndex = 0;
			break;	//	Out of For
		}
	}
}

void ADC_THCMP_IRQHandler(void){

}

void ADC_OVR_IRQHandler(void){

}

Adc::~Adc(){}
*/
