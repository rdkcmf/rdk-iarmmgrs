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

RM          := rm -rf
CFLAGS      := -g -fPIC -D_REENTRANT -Wall
OBJS        := $(patsubst %.c,%.o,$(wildcard *.c))
CPPLAGS      := -g -fPIC -D_REENTRANT -Wall
EXECUTABLE  := deepSleepMgrMain

INCLUDE     =	-I.\
		-I../deepsleepmgr\
		-I../ir\
		-I../ir/include \
		-I../hal/include \
		-I../sysmgr/include \
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \
		-I$(IARM_PATH)/soc/${PLATFORM_SOC}/deepsleep \
		-I$(DS_PATH)/hal/include \
		-I$(DS_PATH)/rpc/include \
		-I$(DS_PATH)/ds/include \
		-I$(LOGGER_PATH)/include \
		-I$(GLIB_HEADER_PATH)/ \
		-I$(GLIB_HEADER_PATH)/glib/ \
		-I$(GLIB_CONFIG_PATH)/../lib/glib-2.0/include/ \
		-I$(RF4CE_PATH)/include 


CFLAGS      += $(INCLUDE) 

CFLAGS += $(_ENABLE_WAKEUP_KEY)
CFLAGS += $(USE_GREEN_PEAK_RF4CE_INTERFACE)
CFLAGS += $(USE_RF4CE_INTERFACE)


LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib

ifeq ($(PLATFORM_SOC),broadcom)

LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus -lnexus

#Adding Deep Sleep Macro so that other platforms are not affected
ifeq ($(COMCAST_PLATFORM), XI3)
CFLAGS       +=-DENABLE_DEEP_SLEEP
endif

endif

ifeq ($(PLATFORM_SOC),intel)
LDFLAGS+=-L$(SDK_FSROOT)/lib -L$(SDK_FSROOT)/usr/lib -L$(SDK_FSROOT)/usr/local/lib/  -L$(CROSS_TOOLCHAIN)/lib
ISMD_LIBS=-L$(TOOLCHAIN_DIR)/lib -lismd_audio
GDL_LIBS=-L$(TOOLCHAIN_DIR)/lib -lgdl
LDFLAGS += -lIARMBus -lrt -L$(DS_PATH)/install/lib -L$(IARM_MGRS)/soc/$(PLATFORM_SOC)/fp -lnspr4 -losal -lrt -lMAF_StackAdapter -lMAF_InputHandler -lMAF_ConfigMgmt -lMAF_DbusAdapter -lpace_spi_drv -lparker_fpd_library -ldshalcli -lds
LDFLAGS += $(ISMD_LIBS)
LDFLAGS += $(GDL_LIBS)
endif

ifeq ($(PLATFORM_SOC),stm)
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus
CFLAGS      += $(INCLUDE)
endif

ifeq ($(PLATFORM_SOC),entropic)
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus
DS_LIBS = ${GLIBS} -lds -ldshalcli -ldshalsrv -ldshal -lcisco_rpc_client -llnxscsUsr -llnxpvrUsr -llnxUKAL -llnxtmvssUsr -llnxnotifyqUsr -lomx -llnxplatUsr -llnxtmvssHDMIRxUDrv -llnxcssUsr 
LDFLAGS += $(DS_LIBS)
endif

LDFLAGS += -L$(DS_PATH)/install/lib -lds -ldshalcli
LDFLAGS += -lIARMBus -L$(IARM_MGRS)/install/lib -lDeepSleepMgr



all:clean objs executable
	@echo "Build Finished...."

objs: $(OBJS)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

executable: $(OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(EXECUTABLE)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
