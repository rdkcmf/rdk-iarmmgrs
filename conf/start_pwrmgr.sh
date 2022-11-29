#!/bin/sh
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
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
