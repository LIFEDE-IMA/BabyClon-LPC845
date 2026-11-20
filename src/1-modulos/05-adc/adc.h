/*
 * my_adc.h
 *
 *  Created on: 23 oct. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#ifndef ADC_H_
#define ADC_H_

#include "LPC845.h"
#include "gpio.h"
#include "statistics.h"

#if defined (__cplusplus)
extern "C" {

void ADC_SEQA_IRQHandler(void);
void ADC_SEQB_IRQHandler(void) ;
void ADC_THCMP_IRQHandler(void);
void ADC_OVR_IRQHandler(void);

}
#endif

class Adc{
	public:
		static const uint8_t TOTAL_ADC_CHANNELS = 12;
		static const uint8_t TOTAL_ADC_SAMPLES = 10;
		static const uint32_t MAX_ADC_FREQ = 1200000;

		enum adcChannel_t : uint8_t{
			ADC_0 = 0,
			ADC_1,
			ADC_2,
			ADC_3,
			ADC_4,
			ADC_5,
			ADC_6,
			ADC_7,
			ADC_8,
			ADC_9,
			ADC_10,
			ADC_11
		};

		enum adcAsyncMode_t : uint8_t{
			SYNC = 0,	//	Synchronous Mode
			ASYNC		//	Asynchronous Mode
		};

		enum adcPwrMode_t : uint8_t{
			NORMAL = 0,
			LPWR		//	The ADC is Automatically Powered-Down when No Conversions Taking Place. The Penalty for Using this Mode is an Approximately 15 ADC Clock Delay
		};

		enum adcSeq_t : uint8_t{
			SEQ_A = 0,
			SEQ_B
		};

		enum adcTrigSrc_t : uint8_t{
			NONE = 0,		//	No HW Trigger
			PININT0_IRQ,	//	GPIO_INT interrupt 0
			PININT1_IRQ,	//	GPIO_INT interrupt 1
			SCT0_OUT3,		//	SCT output 3
			SCT0_OUT4,		//	SCT output 4
			T0_MAT3,		//	CTIMER match 3
			CMP0_OUT_ADC,	//	Analog comparator output
			GPIO_INT_BMAT,	//	GPIO_INT bmatch
			ARM_TXEV		//	ARM core TXEV event
		};

		enum adcTrigPol_t : uint8_t{
			NEGATIVE_EDGE = 0,
			POSITIVE_EDGE
		};

		enum adcRetrievingConversionMethod_t : uint8_t{	//	Reading Global Data Register (SEQx_GDAT) at the End of Each Conversion, or the Individual Channel Result Registers at the End of the Entire Sequence
			END_OF_CONVERSION = 0,	//
			END_OF_SEQUENCE
		};

		enum adcThresholdComparison_t : uint8_t{
			DISABLED = 0,
			OUTSIDE_THRESHOLD,
			CROSSING_THRESHOLD
		};

		enum adcThresholdSet_t : uint8_t{
			THR_SET_0 = 0,
			THR_SET_1
		};

		enum adcThresholdRange_t : uint8_t{
			IN_RANGE = 0,
			BELOW_RANGE,
			ABOVE_RANGE
		};

		enum adcThresholdCross_t : uint8_t{
			NO_CROSS = 0x0,
			DOWNWARD_CROSS = 0x2,
			UPWARD_CROSS = 0x3
		};

		enum adcValueFilter_t : uint8_t{
			F_NONE = 0,
			F_MEAN,
			F_MEDIAN
		};

	private:
		static uint32_t s_seqBase[2];		//	SEQx_CTRL Common Config (For All Channels)
		static bool s_seqConfigured[2];
		static uint16_t s_seqMask[2];		//	Channels Currently Part of Each Sequence
		static uint8_t s_adcChannelsNeeded;	//	Total ADCs Instanced

		adcChannel_t m_adcChannel;

		uint32_t m_freqHz;

		adcSeq_t m_adcSequence;
		adcTrigSrc_t m_adcTrigSrc;
		adcTrigPol_t m_adcTrigPol;
		bool m_burstFlag;		//	True if SEQx Was Configured as Burst
		bool m_singleStepFlag;	//	True if SEQx Was Configured as Single Step
		adcAsyncMode_t m_adcAsyncMode;
		adcPwrMode_t m_adcPwrMode;
		bool m_adcBypassSyncFlag;
		bool m_adcLowPriorityFlag;
		adcRetrievingConversionMethod_t m_adcMode;
		bool m_adcInitFinishedFlag;
		bool m_adcStartedFlag;

		volatile uint16_t m_adcLastSample;	//	ADC RAW Result
		volatile uint16_t m_adcSamplesBuff[Adc::TOTAL_ADC_SAMPLES];	//	ADC RAW Results Buffer
		volatile uint8_t m_adcSamplesBuffIndex;
		volatile uint8_t m_adcValidSamplesCounter;	//	Saturates at TOTAL_ADC_SAMPLES
		volatile bool m_adcNewSampleFlag;

		volatile uint8_t m_adcLastRange;	//	THCMPRANGE of Last Sample
		volatile uint8_t m_adcLastCross;	//	THCMPCROSS of Last Sample
		volatile bool m_adcThFlag;			//	Latched Threshold Event
		volatile uint8_t m_adcThRange;		//	Range Value When Event Fired
		volatile uint8_t m_adcThCross;		//	Cross Value When Event Fired
		volatile uint32_t m_adcThCount;

		volatile bool m_adcOvrFlag;
		volatile uint32_t m_adcOvrCounter;

		void disableIoconPullResistors(void);	//	Disables Pull-Up and Pull-Down Config in IOCON Register

	public:
		Adc(adcChannel_t adcChannel, uint32_t freqHz, adcTrigSrc_t trigSrc = adcTrigSrc_t::NONE,
			adcTrigPol_t trigPol = adcTrigPol_t::POSITIVE_EDGE, bool burst = true, bool singleStep = false,
			adcSeq_t adcSeq = adcSeq_t::SEQ_A, adcAsyncMode_t asyncMode = adcAsyncMode_t::SYNC,
			adcPwrMode_t pwrMode = adcPwrMode_t::NORMAL, bool enableBypassSync = false, bool lowPriority = false,
			adcRetrievingConversionMethod_t modeBit = adcRetrievingConversionMethod_t::END_OF_CONVERSION);	//	Constructor (Just Stores Info, init() Call Is Needed)

		bool init(bool launchADC = true);	//	False if Instances Conflict

		void start(void);	//	Adds Channel to SEQx
		void stop(void);	//	Removes Channel from SEQx

		void enableInt(bool enOvr = true, adcThresholdComparison_t enCmp = adcThresholdComparison_t::DISABLED);		//	Enables SEQx Interrupt and, Depending on [enOvr] and [enCmp], Overrun Interrupt and Threshold Comparison Interrupt
		void disableInt(bool disOvr = true, bool disCmp = true);		//	Disables SEQx Interrupt and, Depending on [disOvr] and [disCmp], Overrun Interrupt and Threshold Comparison Interrupt
		void enableNVIC_int(bool enOvr = true, bool enCmp = false);		//	Enables SEQx NVIC Interrupt and, Depending on [enOvr] and [enCmp], Overrun Interrupt and Threshold Comparison NVIC Interrupt
		void disableNVIC_int(bool disOvr = true, bool disCmp = true);	//	Disables SEQx NVIC Interrupt and, Depending on [disOvr] and [disCmp], Overrun Interrupt and Threshold Comparison NVIC Interrupt

		bool triggerSingleSeqConversion(void);	//	SW Trigger, NO Burst

		//	THCMP
		void setThresholds(uint16_t lowTh, uint16_t highTh, adcThresholdSet_t thSet = adcThresholdSet_t::THR_SET_0);	//	Shared With Every Channel Using The Same SET_X
		bool thresholdEvent(void) const;	//	Latched till clearThreholdEvent()
		adcThresholdRange_t getThresholdRange(void) const;	//	Returns [m_adcThRange]
		adcThresholdCross_t getThresholdCross(void) const;	//	Returns [m_adcThCross]
		uint32_t getThresholdEventCount(void) const;
		void clearThresholdEvent(void);

		//	OVERRUN
		bool overrun(void) const;	//	Latched till clearOverrun()
		uint32_t getOverrunCount(void) const;
		void clearOverrun(void);

		bool newSample(void);	//	True if Theres a Sample Not Read Yet

		adcChannel_t getChannel(void) const;	//	Returns Current Channel
		adcSeq_t getSequence(void) const;

		uint32_t getValue(adcValueFilter_t filter);

		friend void ADC_SEQA_IRQHandler(void);	//	ADC Sequence A Finished ISR Handler
		friend void ADC_SEQB_IRQHandler(void) ;	//	ADC Sequence B Finished ISR Handler
		friend void ADC_THCMP_IRQHandler(void);	//	ADC Threshold Compare ISR Handler
		friend void ADC_OVR_IRQHandler(void);	//	ADC Overrun ISR Handler

		~Adc();	//	Destructor
};

#endif /* MY_ADC_H_ */
