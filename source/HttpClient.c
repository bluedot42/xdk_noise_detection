/* system header files */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "netcfg.h"
#include "em_wdog.h"
#include "em_rmu.h"
#include "em_emu.h"

/* additional interface header files */
#include "OS_operatingSystem_ih.h"
#include "PAL_initialize_ih.h"
#include "PAL_socketMonitor_ih.h"
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include <Serval_HttpClient.h>
#include <Serval_Network.h>
#include "PTD_portDriver_ph.h"
#include "PTD_portDriver_ih.h"
#include "OS_SleepMgt_ih.h"
#include "ADC_ih.h"

/* local header */
#include "HttpClient.h"

OS_timerHandle_tp sendDataTimerHandle;
OS_timerHandle_tp wdgFeedTimerHandle;
Ip_Address_T destAddr = UINT32_C(0);
uint8_t _macVal[WNS_MAC_ADDR_LEN];
uint8_t _macAddressLen = WNS_MAC_ADDR_LEN;
char url_ptr[MAX_URL_SIZE];
Msg_T* msg_ptr = NULL;

uint8_t sampleRawIndex = 0;
uint8_t sampleCacheFullFlag = 0;
uint8_t callCount = 1;
uint32_t SampleRawData[SAMPLE_RAW_COUNT];

uint32_t sound_level_calc(uint32_t mic_sample);
void NLAInit(void);
void NLA_GetSample(void);

//////////////////////////////////////////////////////////////

/* Defining the watchdog initialization data */
WDOG_Init_TypeDef WDG_init =
{
		  .enable     = false,            /* Do not start watchdog when init done */
		  .debugRun   = false,            /* WDOG not counting during debug halt */
		  .em2Run     = true,             /* WDOG counting when in EM2 */
		  .em3Run     = false,             /* WDOG counting when in EM3 */
		  .em4Block   = false,            /* EM4 can be entered */
		  .swoscBlock = true,             /* Block disabling LFRCO/LFXO in CMU */
		  .lock       = false,            /* Do not lock WDOG configuration (if locked, reset needed to unlock) */
		  .clkSel     = wdogClkSelLFXO,   /* Select the 32.768kHZ LFXO oscillator */
		  .perSel     = wdogPeriod_256k,   /* Set the watchdog period to 65537 clock periods (ie ~2 seconds)*/
		};

unsigned long resetCause;

//////////////////////////////////////////////////////////////

retcode_t wlanConnect(void)
{
    NCI_ipSettings_t myIpSettings;
    char ipAddress[PAL_IP_ADDRESS_SIZE] = {0};
    Ip_Address_T* IpaddressHex = Ip_getMyIpAddr();
    WLI_connectSSID_t connectSSID;
    WLI_connectPassPhrase_t connectPassPhrase;

    if(WLI_SUCCESS != WLI_init())
    {
        return(RC_PLATFORM_ERROR);
    }

    printf("Connecting to WLAN: %s \r\n", WLAN_CONNECT_WPA_SSID);

    connectSSID = (WLI_connectSSID_t) WLAN_CONNECT_WPA_SSID;
    connectPassPhrase = (WLI_connectPassPhrase_t) WLAN_CONNECT_WPA_PASS;

    if (WLI_SUCCESS == WLI_connectWPA(connectSSID, connectPassPhrase, NULL))
    {
        NCI_getIpSettings(&myIpSettings);
        *IpaddressHex = Basics_htonl(myIpSettings.ipV4);
        (void)Ip_convertAddrToString(IpaddressHex,(char *)&ipAddress);
        printf("Connected to WLAN successful, IP is: %s \r\n ", ipAddress);

        int8_t _status = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &_macAddressLen, (uint8_t *) _macVal);

		if (WNS_FAILED == _status)
		{
			printf("Obtaining MAC address failed... \r\n");
		}
		else{
			printf("Obtaining MAC address success. MAC is: %02X:%02X:%02X:%02X:%02X:%02X \r\n", _macVal[0],_macVal[1],_macVal[2],_macVal[3],_macVal[4],_macVal[5]);
		}

        return(RC_OK);
    }
    else
    {
        return(RC_PLATFORM_ERROR);
    }
}

retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status)
{
    (void) callfunc;

    if (status != RC_OK)
    {
    	printf("\r\n Error occurred in connecting server \r\n" );
    }
    return(RC_OK);
}

void cleanStack(void){
	msg_ptr = NULL;
	memset(url_ptr, 0, MAX_URL_SIZE);
	memset(SampleRawData, 0, SAMPLE_RAW_COUNT);
	sampleCacheFullFlag = 0;
	sampleRawIndex = 0;
	OS_timerStart(sendDataTimerHandle, TIMERBLOCKTIME);
//	printf("\t End cleaning stack...!\r\n");
}

void wdgFeed(OS_timerHandle_tp xTimer)
{
	(void) (xTimer);
	printf("watchdog feeding\r\n");
	WDOG_Feed();
}

void sendData(OS_timerHandle_tp xTimer)
{
	(void) (xTimer);
	retcode_t rc = RC_OK;
	Ip_Port_T destPort = (Ip_Port_T) DEST_PORT_NUMBER;

	msg_ptr = NULL;

	rc = HttpClient_initRequest(&destAddr, Ip_convertIntToPort(destPort), &msg_ptr);

	if (rc != RC_OK)
	{
	   OS_timerStop(sendDataTimerHandle, TIMERBLOCKTIME);
	   cleanStack();
	   return;
	}
	else if (msg_ptr == NULL)
	{
//	   printf("\tFailed HttpClient_initRequest, msg_ptr NULL, program stop here!\r\n" );
	   rc = RC_APP_ERROR;
//	   printf("\t Start cleaning stack...!\r\n");
	   OS_timerStop(sendDataTimerHandle, TIMERBLOCKTIME);
	   cleanStack();
	   return;
	}

	if(callCount>=255){
		callCount=1;
	}
	printf("===> Send count = %d \r\n", callCount++);

	//get sample
	printf("===> Getting samples... \r\n");
	memset(url_ptr, 0, MAX_URL_SIZE);
	memset(SampleRawData, 0, SAMPLE_RAW_COUNT);
	NLA_GetSample();

	Callable_assign(NULL, &callbackOnSent);

	snprintf(url_ptr,MAX_URL_SIZE,"%s?id=CC-3D-82-65-12-03&value=%ld,%ld,%ld,%ld", DEST_URL_PATH, SampleRawData[4],SampleRawData[9],SampleRawData[14],SampleRawData[15]);

	printf("\tURL_PTR: %s \r\n", url_ptr);
//		printf("req url = %s \r\n", HttpMsg_getReqUrl(msg_ptr));

	HttpMsg_setReqMethod(msg_ptr, Http_Method_Get);

	rc = HttpMsg_setReqUrl(msg_ptr, url_ptr);

	if(rc == RC_HTTP_TOO_LONG_URL){
		printf("\tURL too long error \r\n");
		return;
	}
	else if (rc != RC_OK)
	{
		printf("\tFailed to fill message \r\n");
		return;
	}

	printf("\tMessage successfully filled... continue\r\n");

	HttpClient_pushRequest(msg_ptr, NULL, NULL);
}

extern void init(void)
{
	memset(_macVal, 0, WNS_MAC_ADDR_LEN);
    retcode_t rc = RC_OK;
    /* Store the cause of the last reset, and clear the reset cause register */
    resetCause = RMU_ResetCauseGet();
    RMU_ResetCauseClear();
    /* Enabling clock to the interface of the low energy modules (including the Watchdog)*/
    CMU_OscillatorEnable(cmuOsc_LFXO, true, true);

    /* Check if the watchdog triggered the last reset */
    if (resetCause & RMU_RSTCAUSE_WDOGRST)
    {
      /* Write feedback to lcd */
     printf("watchdog reset last time\r\n");

    }
    /* Starting LFXO and waiting in EM2 until it is stable */
    CMU_OscillatorEnable(cmuOsc_LFXO, true, false);
    while (!(CMU->STATUS & CMU_STATUS_LFXORDY))
    {
      EMU_EnterEM2(false);
    }
    /* Initializing watchdog with choosen settings */
    WDOG_Init(&WDG_init);
    WDOG_Enable(true);
    rc = wlanConnect();
    if(RC_OK != rc )
    {
        printf("Network init/connection failed %i \r\n", rc);
        return;
    }

    rc = PAL_initialize();
    if (RC_OK != rc)
    {
        printf("PAL and network initialize %i \r\n", rc);
        return;
    }

    PAL_socketMonitorInit();

   // start client
    rc = HttpClient_initialize();
    if (rc != RC_OK)
    {
        printf("Failed to initialize http client... \r\n ");
        return;
    }

    if (RC_OK != PAL_getIpaddress((uint8_t*)DEST_HOST, &destAddr))
    {
		return;
    }
    else
    {
    	NLAInit();


		sendDataTimerHandle = OS_timerCreate(
						(const int8_t *) "sendData", CONNECT_TIME_INTERVAL,
						OS_AUTORELOAD_ON, NULL, sendData);

		if (sendDataTimerHandle != NULL)
		{
			OS_timerStart(sendDataTimerHandle, TIMERBLOCKTIME);
		}
		wdgFeedTimerHandle = OS_timerCreate(
				(const int8_t *) "wdgFeed", FEED_TIME_INTERVAL,
				OS_AUTORELOAD_ON, NULL, wdgFeed);
		if (wdgFeedTimerHandle != NULL)
		{
			OS_timerStart(wdgFeedTimerHandle, TIMERBLOCKTIME);
		}
    }
}

/* The description is in the interface header file. */
extern void deinit(void)
{
    /*do nothing*/
}

/**
 * @brief This is a template function where the user can write his custom application.
 *
 */
void appInitSystem(OS_timerHandle_tp xTimer)
{
    (void) (xTimer);
    /*Call the RHC init API */
    init();
}


//sensor part
//void NLA_GetSample(OS_timerHandle_tp xTimer)
void NLA_GetSample(void)
{
//	(void) (xTimer);
	uint32_t mic_sample;

	// Start microphone
	PTD_pinOutSet (PTD_PORT_AKU340_VDD, PTD_PIN_AKU340_VDD);
	sampleRawIndex = 0;

	while(1){

		ADC_Start(ADC0, adcStartSingle);
		while (ADC0->STATUS & ADC_STATUS_SINGLEACT) ;

		mic_sample = 0x7FC & ADC_DataSingleGet(ADC0);
		uint32_t s = sound_level_calc(mic_sample);
		if(s!=SampleRawData[sampleRawIndex-1])
			SampleRawData[sampleRawIndex++] = s;

		if(sampleRawIndex>=SAMPLE_RAW_COUNT) break;
	}

/*	if(sampleRawIndex>=SAMPLE_RAW_COUNT || sampleCacheFullFlag == 1)
	{
//		sampleCacheFullFlag = 1;
		return;
	}*/
//	if(!sampleCacheFullFlag){
//
//
//
//	    sampleCacheFullFlag = 1;
//
//	    //stop sensor timer
//		/*if (NoiseTimerHandle != NULL)
//		{
//			sampleCacheFullFlag = 1;
//
//			printf("\tStop noise timer in SENSOR...!\r\n" );
//			OS_timerStop(NoiseTimerHandle, STOPTIMERBLOCKTIME);
//		}*/
//
//	    return;
//	}
}

void NLAInit(void)
{
	    ADC_Init_TypeDef adc0_init_conf =
	{
    		adcOvsRateSel2,                /* 2x oversampling (if enabled). */
			adcLPFilterBypass,             /* No input filter selected. */
			adcWarmupKeepADCWarm,          /* ADC shutdown after each conversion. */
    		_ADC_CTRL_TIMEBASE_DEFAULT,    /* Use HW default value. */
    		_ADC_CTRL_PRESC_DEFAULT,       /* Use HW default value. */
    		false
    };

    ADC_InitSingle_TypeDef adc0_singleinit_conf =
    {
    		adcPRSSELCh0,
			adcAcqTime1,
			adcRef2V5,
			adcRes12Bit,
			adcSingleInpCh4,
			false,
			false,
			false,
			false
    };

    CMU_ClockEnable(cmuClock_ADC0, true);

    ADC_Init(ADC0, &adc0_init_conf);
    ADC_InitSingle(ADC0, &adc0_singleinit_conf);

/*	NoiseTimerHandle = OS_timerCreate(
				(const int8_t *) "SampleNoisedata", CONNECT_TIME_INTERVAL_NOISE,
				OS_AUTORELOAD_ON, NULL, NLA_GetSample);

	if (NoiseTimerHandle != NULL)
	{
		OS_timerStart(NoiseTimerHandle, TIMERBLOCKTIME);
	}*/

}


uint32_t sound_level_calc(uint32_t mic_sample)
{
	static uint32_t sample_buffer[32];
	static uint8_t  current_sample = 0;
	static uint8_t  i;
	uint32_t sum;

	sample_buffer[current_sample] = mic_sample * mic_sample;
	current_sample = (current_sample +1) % 32;

	for(i=0, sum=0; i<32; i++)
	{
		sum += sample_buffer[i];
	}

	return sum;
}
