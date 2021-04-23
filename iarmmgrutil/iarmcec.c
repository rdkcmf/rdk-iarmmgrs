#include <string.h>
#include "comcastIrKeyCodes.h"
#include "libIBus.h"
#include "videoOutputPort.hpp"
#include "host.hpp"
#include "CecIARMBusMgr.h"
#include "iarmcec.h"
#include "safec_lib.h"

bool IARMCEC_SendCECActiveSource(bool bCecLocalLogic, int keyType, int keyCode)
{
    errno_t rc = -1;
    bool status = 0;
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    IARM_Bus_CECMgr_Send_Param_t dataToSend;
    unsigned char buf[4] = {0x0F, 0x82, 0x00, 0x00}; //Active source event opcode 0x82

    if(bCecLocalLogic && (keyType == KET_KEYDOWN) && (keyCode == KED_MENU))
    {
        //get and update logical address in buf
        IARM_Bus_CECMgr_GetLogicalAddress_Param_t data;
        memset(&data, 0, sizeof(data));
        data.devType = 1;
        ret = IARM_Bus_Call(IARM_BUS_CECMGR_NAME,IARM_BUS_CECMGR_API_GetLogicalAddress,(void *)&data, sizeof(data));
        if( IARM_RESULT_SUCCESS == ret)
        {
            buf[0] |= data.logicalAddress << 4;
            //get and update physical address in buf
             uint8_t physAddress[4] = {0xF,0xF,0xF,0xF};
            try
            {
                device::VideoOutputPort vPort = device::Host::getInstance().getVideoOutputPort("HDMI0");
                if (vPort.isDisplayConnected())
                {
                    vPort.getDisplay().getPhysicallAddress(physAddress[0],physAddress[1],physAddress[2],physAddress[3]);
                    buf[2] |= ((physAddress[0] << 4)|(physAddress[1]));
                    buf[3] |= ((physAddress[2] << 4)|(physAddress[3]));

                    memset(&dataToSend, 0, sizeof(dataToSend));
                    dataToSend.length = 4;
		    rc = memcpy_s(dataToSend.data,sizeof(dataToSend.data), buf, dataToSend.length);
		    if(rc!=EOK)
		    {
			ERR_CHK(rc);
		    }
                    ret = IARM_Bus_Call(IARM_BUS_CECMGR_NAME,IARM_BUS_CECMGR_API_Send,(void *)&dataToSend, sizeof(dataToSend));
                    if( IARM_RESULT_SUCCESS == ret)
                    {
                        LOG("%s: send CEC ActiveSource buf:%x:%x:%x:%x\r\n",__FUNCTION__,buf[0],buf[1],buf[2],buf[3]);
                        if(!vPort.isActive())
                            status = true;
                    }
                    else
                    {
                        LOG("%s: Failed to send CEC ActiveSource ret:%d \r\n",__FUNCTION__,ret);
                    }
                 }
             }
             catch(const std::exception e)
             {
                 LOG("%s: failed to get physAddress\r\n",__FUNCTION__);
             }
        }
        else
        {
            LOG("%s failed to get logicalAddress\r\n",__FUNCTION__);
        }
    }
    return status;
}

bool IARMCEC_SendCECImageViewOn(bool bCecLocalLogic)
{
    errno_t rc = -1;
    bool status = 0;
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    IARM_Bus_CECMgr_Send_Param_t dataToSend;
    unsigned char buf[2] = {0x00, 0x04}; //Image view on  event opcode 0x04

    if(bCecLocalLogic)
    {
        //get and update logical address in buf
        IARM_Bus_CECMgr_GetLogicalAddress_Param_t data;
        memset(&data, 0, sizeof(data));
        data.devType = 1;
        ret = IARM_Bus_Call(IARM_BUS_CECMGR_NAME,IARM_BUS_CECMGR_API_GetLogicalAddress,(void *)&data, sizeof(data));
        if( IARM_RESULT_SUCCESS == ret)
        {
            buf[0] |= data.logicalAddress << 4;
            //get and update physical address in buf

            memset(&dataToSend, 0, sizeof(dataToSend));
            dataToSend.length = 2;
	    rc = memcpy_s(dataToSend.data,sizeof(dataToSend.data), buf, dataToSend.length);
	    if(rc!=EOK)
	    {
	          ERR_CHK(rc);
	    }
            ret = IARM_Bus_Call(IARM_BUS_CECMGR_NAME,IARM_BUS_CECMGR_API_Send,(void *)&dataToSend, sizeof(dataToSend));
            if( IARM_RESULT_SUCCESS == ret)
            {
                 LOG("%s: send CEC ImageViewOn buf:%x:%x\r\n",__FUNCTION__,buf[0],buf[1]);
                 status = true;
            }
        }
        else
        {
            LOG("%s failed to get logicalAddress\r\n",__FUNCTION__);
        }
    }
    return status;
}
