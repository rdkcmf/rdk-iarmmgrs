#ifndef _IARM_BUS_RECEIVERMGR_H
#define _IARM_BUS_RECEIVERMGR_H


#ifdef __cplusplus
extern "C"
{
#endif

#define IARM_RECEIVER_NAME "XRE_native_receiver"

#define IARM_RECEIVER_GET_APP_INFO "XRE_RECEIVER_getAppInfo"
#define IARM_RECEIVER_SET_CONNECTION_RESET "XRE_RECEIVER_setConnectionReset"

#define APP_INFO_JSON_MAX 4096

typedef struct _AppInfoData {
    char appInfoJson[APP_INFO_JSON_MAX];
} AppInfoData;

#define CONNECTION_ID_MAX 1024
typedef struct _ConnectionResetData {
    char applicationID[CONNECTION_ID_MAX];
    char connectionID[CONNECTION_ID_MAX];
    char connectionResetLevel[CONNECTION_ID_MAX];
} ConnectionResetData;

typedef struct _IARM_Bus_Receiver_Param_t {
    union {
        AppInfoData appInfoData;
        ConnectionResetData connectionResetData;
    } data;
    int status;
} IARM_Bus_Receiver_Param_t;

#ifdef __cplusplus
}
#endif

#endif



