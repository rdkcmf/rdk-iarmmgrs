#!/bin/bash
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

#######################################
#
# Build Framework standard script for
#
# IARMMgrs component

# use -e to fail on any shell issue
# -e is the requirement from Build Framework
set -e


# default PATHs - use `man readlink` for more info
# the path to combined build
export RDK_PROJECT_ROOT_PATH=${RDK_PROJECT_ROOT_PATH-`readlink -m ..`}/
export COMBINED_ROOT=$RDK_PROJECT_ROOT_PATH

# path to build script (this script)
export RDK_SCRIPTS_PATH=${RDK_SCRIPTS_PATH-`readlink -m $0 | xargs dirname`}/

# path to components sources and target
export RDK_SOURCE_PATH=${RDK_SOURCE_PATH-`readlink -m .`}/
export RDK_TARGET_PATH=${RDK_TARGET_PATH-$RDK_SOURCE_PATH}

# fsroot and toolchain (valid for all devices)
export RDK_FSROOT_PATH=${RDK_FSROOT_PATH-`readlink -m $RDK_PROJECT_ROOT_PATH/sdk/fsroot/ramdisk`}/
export RDK_TOOLCHAIN_PATH=${RDK_TOOLCHAIN_PATH-`readlink -m $RDK_PROJECT_ROOT_PATH/sdk/toolchain/staging_dir`}


# default component name
export RDK_COMPONENT_NAME=${RDK_COMPONENT_NAME-`basename $RDK_SOURCE_PATH`}


# parse arguments
INITIAL_ARGS=$@

function usage()
{
    set +x
    echo "Usage: `basename $0` [-h|--help] [-v|--verbose] [action]"
    echo "    -h    --help                  : this help"
    echo "    -v    --verbose               : verbose output"
    echo "    -p    --platform  =PLATFORM   : specify platform for IARMMgrs"
    echo
    echo "Supported actions:"
    echo "      configure, clean, build (DEFAULT), rebuild, install"
}

# options may be followed by one colon to indicate they have a required argument
if ! GETOPT=$(getopt -n "build.sh" -o hvp: -l help,verbose,platform: -- "$@")
then
    usage
    exit 1
fi

eval set -- "$GETOPT"

while true; do
  case "$1" in
    -h | --help ) usage; exit 0 ;;
    -v | --verbose ) set -x ;;
    -p | --platform ) CC_PLATFORM="$2" ; shift ;;
    -- ) shift; break;;
    * ) break;;
  esac
  shift
done

ARGS=$@


# component-specific vars
export RDK_PLATFORM_SOC=${RDK_PLATFORM_SOC-broadcom}
export CC_PLATFORM=${CC_PLATFORM-$RDK_PLATFORM_SOC}
export PLATFORM_SOC=$CC_PLATFORM
CC_PATH=$RDK_SOURCE_PATH
export FSROOT=${RDK_FSROOT_PATH}
export TOOLCHAIN_DIR=${RDK_TOOLCHAIN_PATH}
export WORK_DIR=$RDK_PROJECT_ROOT_PATH/work${RDK_PLATFORM_DEVICE^^}
export BUILDS_DIR=$RDK_PROJECT_ROOT_PATH


# functional modules

function configure()
{
   if [ $RDK_PLATFORM_SOC = "mstar" ]; then
	  cp $COMBINED_ROOT/iarmmgrs/soc/mstar/trunk/ir/Makefile $COMBINED_ROOT/iarmmgrs/generic/ir/
	  cp $COMBINED_ROOT/iarmmgrs/soc/mstar/trunk/ir/nec_keydef.h $COMBINED_ROOT/iarmmgrs/generic/ir/
	  cp $COMBINED_ROOT/iarmmgrs/soc/mstar/trunk/ir/plat.c $COMBINED_ROOT/iarmmgrs/generic/ir/
	  cp $COMBINED_ROOT/iarmmgrs/soc/mstar/trunk/Makefile $COMBINED_ROOT/iarmmgrs/generic/
   else
	 true #use this function to perform any pre-build configuration
   fi
}

function clean()
{
    true #use this function to provide instructions to clean workspace
}

function build()
{
    IARMMGRS_PATH=${RDK_SCRIPTS_PATH}
    IARMMGRSGENERIC_PATH=${IARMMGRS_PATH}
    cd $IARMMGRSGENERIC_PATH

   
    export DS_PATH=$BUILDS_DIR/devicesettings
    export IARM_PATH=$BUILDS_DIR/iarmbus
    export IARM_MGRS=$BUILDS_DIR/iarmmgrs
    export LOGGER_PATH=$BUILDS_DIR/logger
    export CEC_PATH=$BUILDS_DIR/hdmicec
    export FULL_VERSION_NAME_VALUE="\"$IMAGE_NAME\""
	export SDK_FSROOT=$COMBINED_ROOT/sdk/fsroot/ramdisk

if [ $RDK_PLATFORM_SOC = "intel" ]; then
	export TOOLCHAIN_DIR=$COMBINED_ROOT/sdk/toolchain/staging_dir
	export CROSS_TOOLCHAIN=$TOOLCHAIN_DIR
	export CROSS_COMPILE=$CROSS_TOOLCHAIN/bin/i686-cm-linux
	export CC=$CROSS_COMPILE-gcc
	export CXX=$CROSS_COMPILE-g++
	export OPENSOURCE_BASE=$COMBINED_ROOT/opensource
	export DFB_ROOT=$TOOLCHAIN_DIR
	export DFB_LIB=$TOOLCHAIN_DIR/lib
	export FSROOT=$COMBINED_ROOT/sdk/fsroot/ramdisk
	export MFR_PATH=$COMBINED_ROOT/ri/mpe_os/platforms/intel/groveland/mfrlibs
	export MFR_FPD_PATH=$COMBINED_ROOT/mfrlibs
	export GLIB_INCLUDE_PATH=$CROSS_TOOLCHAIN/include/glib-2.0/
	export GLIB_LIBRARY_PATH=$CROSS_TOOLCHAIN/lib/
    	export GLIB_CONFIG_INCLUDE_PATH=$GLIB_LIBRARY_PATH/glib-2.0/
	export GLIBS='-lglib-2.0'
	export _ENABLE_WAKEUP_KEY=-D_ENABLE_WAKEUP_KEY
	export _INIT_RESN_SETTINGS=-D_INIT_RESN_SETTINGS
	export RF4CE_API="-DRF4CE_API -DUSE_UNIFIED_RF4CE_MGR_API_4"
	export RF4CE_PATH=$COMBINED_ROOT/rf4ce/generic/
	export CURL_INCLUDE_PATH=$OPENSOURCE_BASE/include/
	export CURL_LIBRARY_PATH=$OPENSOURCE_BASE/lib/
	export LDFLAGS="$LDFLAGS -Wl,-rpath-link=$RDK_FSROOT_PATH/usr/local/lib"
        export GLIB_HEADER_PATH=$CROSS_TOOLCHAIN/include/glib-2.0/
        export GLIB_CONFIG_PATH=$CROSS_TOOLCHAIN/lib/

elif [ $RDK_PLATFORM_SOC = "broadcom" ]; then
    echo "building for pace ${RDK_PLATFORM_DEVICE^^}..." 
    export WORK_DIR=$BUILDS_DIR/work${RDK_PLATFORM_DEVICE^^}


		if [ ${RDK_PLATFORM_DEVICE} = "rng150" ];then
			if [ -f $BUILDS_DIR/sdk/scripts/setBcmEnv.sh ]; then
				. $BUILDS_DIR/sdk/scripts/setBcmEnv.sh		
			fi
			if [ -f $BUILDS_DIR/sdk/scripts/setBCMenv.sh ]; then
				. $BUILDS_DIR/sdk/scripts/setBCMenv.sh		
			fi
		else
  		          source ${RDK_PROJECT_ROOT_PATH}/build_scripts/setBCMenv.sh
		fi

	
	export COMCAST_PLATFORM=${COMCAST_PLATFORM}
    export OEM_VENDOR=${OEM_VENDOR}
    export GLIB_HEADER_PATH=${WORK_DIR}/rootfs/usr/local/include/glib-2.0/
    export GLIB_CONFIG_PATH=${WORK_DIR}/rootfs/usr/local/lib

    if [ ${RDK_PLATFORM_VENDOR}${RDK_PLATFORM_DEVICE} = "samsungxg1" ]; then
        export MFR_PATH=${WORK_DIR}/svn/sdk/mfrlib/lib
        export MFR_INCLUDE_PATH=${MFR_PATH}/include
        export MFR_LIB_PATH=${MFR_PATH}/bin
        export IARM_INCLUDE_PATH=${IARM_PATH}/core/include
        export BSEAV_INCLUDE_PATH=${BSEAV_DIR}
        export NXCLIENT_INCLUDE_PATH=${WORK_DIR}/Refsw/nexus/nxclient/include
        export NEXUS_INCLUDE_PATH=${NEXUS_BIN_DIR}/include
        export SYSROOT_INCLUDE_PATH=${WORK_DIR}/rootfs/usr/local/include
        export SYSROOT_LIB_PATH=${WORK_DIR}/rootfs/usr/local/lib
        export LIBNAME_SUFFIX=.so
        export MFRMGR_INCLUDE_PATH=${WORK_DIR}/svn/sdk/mfrlib/mfrmgr/include
        export MFRMGR_LIB_PATH=${WORK_DIR}/svn/sdk/mfrlib/mfrmgr
        export DSGCCMGR_INCLUDE_PATH=${WORK_DIR}/svn/sdk/mfrlib/dsgccmgr/include
        export DSGCCMGR_LIB_PATH=${WORK_DIR}/svn/sdk/mfrlib/dsgccmgr
    else
        export MFR_PATH=$WORK_DIR/svn/sdk/mfrlib
    fi
    export OPENSOURCE_BASE=$BUILDS_DIR/opensource
    CROSS_COMPILE=mipsel-linux
    export CC=$CROSS_COMPILE-gcc
    export CXX=$CROSS_COMPILE-g++
    export CURL_INCLUDE_PATH=$APPLIBS_TARGET_DIR/usr/local/include/
    export CURL_LIBRARY_PATH=$APPLIBS_TARGET_DIR/usr/local/lib/
    export GLIB_INCLUDE_PATH=$SDK_FSROOT/usr/local/include/glib-2.0/
    export GLIB_LIBRARY_PATH=$SDK_FSROOT/usr/local/lib/
    export GLIBS='-lglib-2.0 -lintl -lz'
    export RF4CE_PATH=$COMBINED_ROOT/rf4ce/generic/
    export OPENSSL_SRC=${RDK_PROJECT_ROOT_PATH}/opensource/openssl
    export OPENSSL_CFLAGS="-I${OPENSSL_SRC}/include"
    export OPENSSL_LDFLAGS="${OPENSSL_SRC}/libcrypto.a"
    if [ ! {$OEM_VENDOR} = "BRCM" ]; then 
        # Do not add this for broadcom reference platforms
        # Device specif flags
        case "${RDK_PLATFORM_DEVICE}" in
        "rng150" ) 
		export _ENABLE_WAKEUP_KEY=-D_ENABLE_WAKEUP_KEY
				
         ;;
        "xi3" ) 
        export RF4CE_API=-DRF4CE_API
        export _ENABLE_WAKEUP_KEY=-D_ENABLE_WAKEUP_KEY
        export _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT=-D_ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
	    export USE_CEC_FEATURE=n
	    export USE_MFR_FOR_SERIAL=y
				
         ;;
       "xg1" ) 
	    export RF4CE_API=-DRF4CE_API
        export _ENABLE_WAKEUP_KEY=-D_ENABLE_WAKEUP_KEY
				
         ;;
        esac
	fi
elif [ $RDK_PLATFORM_SOC = "entropic" ]; then
       echo "CC_PLATFORM=$CC_PLATFORM"
       export SDK_CONFIG=stb597_V3_xi3
       export BUILD_DIR=$BUILDS_DIR
       source ${BUILDS_DIR}/build_scripts/setupSDK.sh

       export FSROOT="${BUILD_DIR}/fsroot"
       export CC="${GCC_PREFIX}-gcc --sysroot ${_TMSYSROOT} -I${_TMTGTBUILDROOT}/comps/generic_apps/usr/include
       export CXX="${GCC_PREFIX}-g++ --sysroot ${_TMSYSROOT} -I${_TMTGTBUILDROOT}/comps/generic_apps/usr/include
       export LD="${GCC_PREFIX}-ld --sysroot ${_TMSYSROOT}"
       export LDFLAGS="-L${GCC_BASE}/lib -L${_TMTGTBUILDROOT}/comps/generated/lib/armgnu_linux_el_cortex-a9 -L${_TMTGTBUILDROOT}/comps/generic_apps/usr/lib -L${FSROOT}/usr/lib -L${FSROOT}/usr/local/lib -lz"

       export OPENSOURCE_BASE="${BUILD_DIR}/opensource"
       export IARM_MGR_PATH="${BUILD_DIR}/iarmmgrs"
       export DFB_LIB="${_TMTGTBUILDROOT}/comps/generic_apps/usr/lib"
       export DFB_ROOT="${_TMTGTBUILDROOT}/comps/generic_apps"
       export GLIB_INCLUDE_PATH="${_TMTGTBUILDROOT}/comps/generic_apps/usr/include/glib-2.0"
       export GLIB_CONFIG_INCLUDE_PATH="${_TMTGTBUILDROOT}/comps/generic_apps/usr/lib/glib-2.0"
       export GLIB_LIBRARY_PATH="${FSROOT}/appfs/lib"
       export GLIBS="-lglib-2.0"
       export DS_PATH="${BUILD_DIR}/devicesettings"
       export LOGGER_PATH="${BUILD_DIR}/logger"
       export IARM_MGRS="${BUILD_DIR}/iarmmgrs"
       export IARM_PATH="${BUILD_DIR}/iarmbus"
       export LOGGER_PATH="${BUILD_DIR}/logger"
       cd "${IARM_MGR_PATH}/generic"
   elif [ $RDK_PLATFORM_SOC = "mstar" ]; then	
	  CROSS_COMPILE=arm-none-linux-gnueabi
	  export CC=$CROSS_COMPILE-gcc
	  export CXX=$CROSS_COMPILE-g++   
	  export GLIBS='-lglib-2.0 -lz'	  
	  export IARM_PATH=$COMBINED_ROOT/iarmbus
      FSROOT=$COMBINED_ROOT/sdk/fsroot
	  export DFB_ROOT=${FSROOT}
	  export GLIB_INCLUDE_PATH="${FSROOT}/usr/include/glib-2.0"
	  export GLIB_CONFIG_INCLUDE_PATH="${FSROOT}/usr/lib/glib-2.0"
	  export LDFLAGS+="-L${FSROOT}/vendor/lib \
	  -L${FSROOT}/usr/lib \
	  -L${IARM_PATH}/install \
	  -L${FSROOT}/vendor/lib/utopia \
	  -lIARMBus \
	  -llinux"
	  export CFLAGS+="-I${FSROOT}/usr/include/utopia"
    elif [ $RDK_PLATFORM_SOC = "stm" ]; then
        source $RDK_SCRIPTS_PATH/../soc/stm/build/soc_env.sh
        export _ENABLE_WAKEUP_KEY=-D_ENABLE_WAKEUP_KEY
    fi
   if [ -f $COMBINED_ROOT/rdklogger/build/lib/librdkloggers.so ]; then
	export RDK_LOGGER_ENABLED='y'
    fi

   if [ $RDK_PLATFORM_SOC = "intel" ] || [ ${RDK_PLATFORM_VENDOR}${RDK_PLATFORM_DEVICE} = "samsungxg1" ] || [ ${RDK_PLATFORM_VENDOR}${RDK_PLATFORM_DEVICE} = "ciscoxg1" ]; then
        export VREX_SUPPORT=novrexmgr
   fi

    make
}

function rebuild()
{
    clean
    build
}

function install()
{
    IARMMGRSINSTALL_PATH=${RDK_SCRIPTS_PATH}/../install

	RAMDISK_TARGET=${RDK_FSROOT_PATH}/lib
    mkdir -p $RAMDISK_TARGET


    cd $IARMMGRSINSTALL_PATH

	cp -v lib/libirInput.so ${RDK_FSROOT_PATH}lib
	cp -v lib/libPwrMgr.so ${RDK_FSROOT_PATH}lib

	if [ $RDK_PLATFORM_SOC = "intel" ]; then
		cp -v bin/irMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/pwrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env    
		cp -v bin/sysMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/dsMgrMain  ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/vrexPrefs.json ${RDK_FSROOT_PATH}mnt/nfs/env
		if [ -e bin/vrexMgrMain ]; then
			cp -v bin/vrexMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		fi
		if [ -e bin/deviceUpdateMgrMain ]; then
			cp -v bin/deviceUpdateMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
			cp -v bin/deviceUpdateConfig.json ${RDK_FSROOT_PATH}mnt/nfs/env
		fi		
	elif [ $RDK_PLATFORM_SOC = "broadcom" ]; then
		cp -v bin/irMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/pwrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env    
		cp -v bin/sysMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/dsMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		if [ ${RDK_PLATFORM_DEVICE} != "rng150" ];then
			if [ ${RDK_PLATFORM_DEVICE} = "xg1" ];then
				cp -v bin/vrexPrefs.json ${RDK_FSROOT_PATH}etc
			fi
			if [ -e bin/vrexMgrMain ]; then
				cp -v bin/vrexMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
				cp -v bin/vrexPrefs.json ${RDK_FSROOT_PATH}mnt/nfs/env
			fi
			if [ -e bin/deviceUpdateMgrMain ]; then
				cp -v bin/deviceUpdateMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
				cp -v bin/deviceUpdateConfig.json ${RDK_FSROOT_PATH}mnt/nfs/env
			fi		
		fi		
		if [ $RDK_PLATFORM_DEVICE = "xi3" ]; then
			if [ "$MFR_MGR_SUPPORT" != "nomfrmgr" ]; then
				cp -v bin/mfrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
				cp -v bin/mfr_deletePDRI ${RDK_FSROOT_PATH}usr/local/bin
				cp -v bin/mfr_scrubAllBanks ${RDK_FSROOT_PATH}usr/local/bin
			fi
		cp -v bin/tr69BusMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/dsMgrMain ${RDK_FSROOT_PATH}usr/local/bin
		cp -v bin/deepSleepMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env    
		cp -v lib/libDeepSleepMgr.so ${RDK_FSROOT_PATH}lib
		fi
		if [ $MFR_MGR = "PACE_7435" ]; then
			cp -v bin/mfrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		fi
		if [ $MFR_NAME = "USE_MOT_MFR" ]; then
			if [ "$MFR_MGR_SUPPORT" != "nomfrmgr" ]; then
				cp -v bin/mfrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
			fi
		fi
		
		if [ ${RDK_PLATFORM_VENDOR}${RDK_PLATFORM_DEVICE} = "pacerng150" ];then
			if [ -e bin/mfrMgrMain ]; then
				cp -v bin/mfrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
			fi	
		fi	

		if [ ${RDK_PLATFORM_VENDOR}${RDK_PLATFORM_DEVICE} = "samsungxg1" ]; then
			cp -v bin/samMfrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
			cp -v lib/libSAMSUNGMfrApi.so ${RDK_FSROOT_PATH}lib
			cp -v lib/libSAMSUNGDsgccApi.so ${RDK_FSROOT_PATH}lib
		fi

	elif [ $RDK_PLATFORM_SOC = "entropic" ]; then
		cp -v bin/irMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/pwrMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env    
		cp -v bin/sysMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
		cp -v bin/dsMgrMain ${RDK_FSROOT_PATH}usr/local/bin
		cp -v bin/tr69BusMain ${RDK_FSROOT_PATH}mnt/nfs/env
	elif [ $RDK_PLATFORM_SOC = "mstar" ]; then	
		cp -v bin/irMgrMain ${RDK_FSROOT_PATH}mnt/nfs/env
	elif [ $RDK_PLATFORM_SOC = "stm" ]; then	
		cp -v bin/*Main ${RDK_FSROOT_PATH}mnt/nfs/env
	        cp -v bin/dsMgrMain ${RDK_FSROOT_PATH}usr/local/bin
		if [ -e bin/vrexPrefs.json ]; then
                	cp -v bin/vrexPrefs.json ${RDK_FSROOT_PATH}mnt/nfs/env
               	fi
               	if [ -e bin/deviceUpdateConfig.json ]; then
              		cp -v bin/deviceUpdateConfig.json ${RDK_FSROOT_PATH}mnt/nfs/env
        	fi
	fi
    
}


# run the logic

#these args are what left untouched after parse_args
HIT=false

for i in "$ARGS"; do
    case $i in
        configure)  HIT=true; configure ;;
        clean)      HIT=true; clean ;;
        build)      HIT=true; build ;;
        rebuild)    HIT=true; rebuild ;;
        install)    HIT=true; install ;;
        *)
            #skip unknown
        ;;
    esac
done

# if not HIT do build by default
if ! $HIT; then
  build
fi
