#!/bin/sh
# ============================================================================
# RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
# ============================================================================
# This file (and its contents) are the intellectual property of RDK Management, LLC.
# It may not be used, copied, distributed or otherwise  disclosed in whole or in
# part without the express written permission of RDK Management, LLC.
# ============================================================================
# Copyright (c) 2021 RDK Management, LLC. All rights reserved.
# ============================================================================
#
. /etc/device.properties

pwrMgr2Enable=`tr181Set Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Power.PwrMgr2.Enable 2>&1 > /dev/null`

if [ "$pwrMgr2Enable" == "true" ]; then
    /bin/systemctl stop deepsleepmgr.service
    rm -rf /tmp/pwrmgr1
    touch /tmp/pwrMgr2
    echo "Enabling New pwrmgr2."
else
    rm -rf /tmp/pwrMgr2
    touch /tmp/pwrMgr1
    echo "Enabling pwrmgr."
fi
