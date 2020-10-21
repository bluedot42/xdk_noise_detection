#ifndef XDK110_HTTPCLIENT_H_
#define XDK110_HTTPCLIENT_H_

#define CONNECT_TIME_INTERVAL           UINT32_C(15000)          /** HTTP REQUEST INTERVAL */
#define FEED_TIME_INTERVAL				UINT32_C(1000)
#define TIMERBLOCKTIME                  UINT32_C(0xffff)        /**< Macro used to define blocktime of a timer*/
#define SAMPLE_RAW_COUNT     			UINT8_C(20)

#define DEST_HOST						"139.196.241.113"
#define DEST_PORT_NUMBER        		UINT16_C(80)
#define DEST_URL_PATH					"/Canteen/api/CanteenService/AddOneImensionalInfoList"
#define MAX_URL_SIZE					255
#define WNS_FAILED 			   			INT32_C(-1)
#define WNS_MAC_ADDR_LEN 	   			UINT8_C(6)
#define WNS_DEFAULT_MAC		            "00:00:00:00:00:00"
//#define WLAN_CONNECT_WPA_SSID           "wsky2352"
//#define WLAN_CONNECT_WPA_PASS           "wsky12191129"
//#define WLAN_CONNECT_WPA_SSID           "CMCC2"
//#define WLAN_CONNECT_WPA_PASS           "mayday9999"
#define WLAN_CONNECT_WPA_SSID           "HUAWEI-IOT-AP"
#define WLAN_CONNECT_WPA_PASS           "mayday9999"

void sendData(OS_timerHandle_tp xTimer);
void wdgData(OS_timerHandle_tp xTimer);
static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status);

static retcode_t wlanConnect(void);

#endif
