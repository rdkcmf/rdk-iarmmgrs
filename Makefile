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
# List of Libraries
install_dir := ../install/bin
install_lib_dir := ../install/lib

# List of Executable
exe_ir              := ir/
exe_power           := power/
ifeq ($(COMCAST_PLATFORM),XI3)
ifeq ($(PLATFORM_SOC),broadcom)
exe_deepsleep       := deepsleepmgr/
endif
endif
exe_sysmgr          := sysmgr/
ifneq ($(COMCAST_PLATFORM), RNG150)
exe_vrexmgr         := vrexmgr/
exe_deviceupdatemgr := deviceUpdateMgr/
endif
ifeq ($(OEM_VENDOR), SAMSUNG)
ifeq ($(COMCAST_PLATFORM), XG2)
exe_dsgcc           := $(MFR_PATH)/../dsgccmgr
endif
exe_mfr             := $(MFR_PATH)/../mfrmgr
else
exe_mfr             := mfr/
endif
exe_ds              := dsmgr/
exe_tr69Bus         := tr69Bus/
exe_test            := test
exe_mfr_test        := mfr/test_mfr
exe_platform_ir     := ../soc/${PLATFORM_SOC}/ir
exe_platform_power  := ../soc/${PLATFORM_SOC}/power
exe_platform_fp     := ../soc/${PLATFORM_SOC}/fp
ifeq ($(COMCAST_PLATFORM),XI3)
ifeq ($(PLATFORM_SOC),broadcom)
exe_platform_deepsleep  := ../soc/${PLATFORM_SOC}/deepsleep
endif
endif

ifneq ($(MFR_MGR_SUPPORT),nomfrmgr)
executable := $(exe_platform_ir) $(exe_platform_power) $(exe_platform_fp) $(exe_ir) $(exe_power) $(exe_sysmgr) $(exe_tr69Bus) $(exe_test) $(exe_mfr) $(exe_ds)
ifeq ($(COMCAST_PLATFORM), XI3)
executable += $(exe_mfr_test)
endif 
else	
executable := $(exe_platform_ir) $(exe_platform_power) $(exe_platform_fp) $(exe_ir) $(exe_power) $(exe_sysmgr) $(exe_tr69Bus) $(exe_test) $(exe_ds)
endif	

ifneq ($(COMCAST_PLATFORM), RNG150)
ifneq ($(VREX_SUPPORT), novrexmgr)
executable += $(exe_vrexmgr)
endif
executable += $(exe_deviceupdatemgr)
endif

ifeq ($(OEM_VENDOR)$(COMCAST_PLATFORM), PACERNG150)
executable += $(exe_mfr)
endif

ifeq ($(OEM_VENDOR)$(COMCAST_PLATFORM), SAMSUNGRNG150)
executable += $(exe_mfr)
endif
ifeq ($(OEM_VENDOR)$(COMCAST_PLATFORM), SAMSUNGXG2)
executable := $(exe_dsgcc) $(executable)
endif

ifeq ($(COMCAST_PLATFORM),XI3)
ifeq ($(PLATFORM_SOC),broadcom)
executable += $(exe_platform_deepsleep) $(exe_deepsleep)
endif
endif

.PHONY: clean all $(executable) install

all: clean $(executable) install 

$(executable):
	$(MAKE) -C $@

install:
	echo "Creating directory.."
	mkdir -p $(install_dir)
	mkdir -p $(install_lib_dir)
	echo "Copying files now.."	
ifneq ($(COMCAST_PLATFORM), RNG150)
	cp $(exe_vrexmgr)/vrexPrefs.json $(install_dir)
ifneq ($(VREX_SUPPORT), novrexmgr)
	cp $(exe_vrexmgr)/*Main $(install_dir)
endif
	cp $(exe_deviceupdatemgr)/*Main $(install_dir)
	cp $(exe_deviceupdatemgr)/deviceUpdateConfig.json $(install_dir)
endif
	cp $(exe_sysmgr)/*Main $(install_dir)
	cp $(exe_power)/*Main $(install_dir)
ifeq ($(COMCAST_PLATFORM),XI3)
ifeq ($(PLATFORM_SOC),broadcom)
	cp $(exe_deepsleep)/*Main $(install_dir)
endif
endif
	cp $(exe_ir)/*Main $(install_dir)
ifneq ($(MFR_MGR_SUPPORT),nomfrmgr)
ifeq ($(MFR_NAME),USE_MOT_MFR)
	cp $(exe_mfr)/*Main $(install_dir)
endif
endif
ifneq ($(PLATFORM_SOC),entropic)
	cp $(exe_platform_ir)/*.so $(install_lib_dir)
endif
	cp $(exe_ds)/*Main $(install_dir)
	cp $(exe_tr69Bus)/*Main $(install_dir)
ifeq ($(COMCAST_PLATFORM), XI3)
ifneq ($(MFR_MGR_SUPPORT),nomfrmgr)
	cp $(exe_mfr)/*Main $(install_dir)
	cp $(exe_mfr_test)/mfr_deletePDRI $(install_dir)
	cp $(exe_mfr_test)/mfr_scrubAllBanks $(install_dir)
endif
endif
ifeq ($(COMCAST_PLATFORM), XG1v3)
ifeq ($(OEM_VENDOR), PACE)
	cp $(exe_mfr)/*Main $(install_dir)
endif
endif

ifeq ($(OEM_VENDOR)$(COMCAST_PLATFORM), PACERNG150)
	cp $(exe_mfr)/*Main $(install_dir)
endif

ifeq ($(COMCAST_PLATFORM), XG2)
ifeq ($(OEM_VENDOR), PACE)
	cp $(exe_mfr)/*Main $(install_dir)
endif
endif
ifeq ($(PLATFORM_SOC),intel)
	cp $(exe_ds)/*Main $(install_dir)
endif
ifeq ($(COMCAST_PLATFORM), XI4)
	cp $(exe_mfr)/*Main $(install_dir)
endif
ifeq ($(OEM_VENDOR)$(COMCAST_PLATFORM), SAMSUNGXG2)
	cp $(exe_dsgcc)/*.so $(install_lib_dir)
	cp $(exe_dsgcc)/*.a $(install_lib_dir)
	cp $(exe_mfr)/*.so $(install_lib_dir)
	cp $(exe_mfr)/*Main $(install_dir)
	mkdir -p $(MFR_PATH)/../bin/release
	cp $(exe_mfr)/*.so $(MFR_PATH)/../bin/release
endif
clean:
	rm -rf $(install_dir)
	rm -rf $(install_lib_dir)
	

