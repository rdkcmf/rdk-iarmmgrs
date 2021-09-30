#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <unistd.h>
#include <glib.h>
#include "pwrlogger.h"
#include "mfrTypes.h"
#include "mfrMgr.h"
#include "productTraits.h"
#include "frontPanelIndicator.hpp"

using namespace pwrMgrProductTraits;
extern int _SetAVPortsPowerState(IARM_Bus_PWRMgr_PowerState_t powerState);
/*
    Following profiles are supported:
    * default-stb
    * default-tv
    * default-stb-eu
    * default-tv-eu
*/
const unsigned int REBOOT_REASON_RETRY_INTERVAL_SECONDS = 2;
ux_controller * ux_controller::singleton = nullptr;
static reboot_type_t getRebootType()
{
    const char * file_updated_flag = "/tmp/Update_rebootInfo_invoked";
    const char * reboot_info_file_name = "/opt/secure/reboot/previousreboot.info";
    const char * hard_reboot_match_string = R"("reason":"POWER_ON_RESET")";
    reboot_type_t ret = reboot_type_t::SOFT;

    //Check whether file has been updated first. If not, the data we read from the file is not valid.
    if(0 != access(file_updated_flag, F_OK))
    {
        LOG("%s: Error! Reboot info file isn't updated yet.\n", __func__);
        ret = reboot_type_t::UNAVAILABLE;
    }
    else
    {
        std::ifstream reboot_info_file(reboot_info_file_name);
        std::string line;
        if (true == reboot_info_file.is_open())
        {
            while(std::getline(reboot_info_file, line))
            {
                if(std::string::npos != line.find(hard_reboot_match_string))
                {
                   LOG("%s: Detected hard reboot.\n", __func__);
                    ret = reboot_type_t::HARD;
                    break;
                }
            }
        }
        else
            LOG("%s: Failed to open file\n", __func__);
    }
    return ret;
}

static gboolean reboot_reason_cb(gpointer data)
{
    static unsigned int count = 0;
    constexpr unsigned int max_count = 120 / REBOOT_REASON_RETRY_INTERVAL_SECONDS; //Stop after 2 minutes.

    reboot_type_t reboot_type = getRebootType();
    if(reboot_type_t::UNAVAILABLE == reboot_type)
    {
        if(max_count >= count++)
        {
            return G_SOURCE_CONTINUE;
        }
        else
        {
            LOG("%s: Exceeded retry limit.\n", __func__);
            return G_SOURCE_REMOVE;
        }
    }
    else
    {
        LOG("%s: Got reboot reason in async check. Applying display configuration.\n", __func__);
        ux_controller * ptr = static_cast <ux_controller *> (data);
        ptr->sync_display_ports_with_reboot_reason(reboot_type);
        return G_SOURCE_REMOVE;
    }
}

static bool isTVOperatingInFactory() //TODO: This is a placeholder.
{
    bool ret = true;
    const char * tv_activated_flag_filename = "/opt/www/authService/as.dat";
    if(0 == access(tv_activated_flag_filename, F_OK))
        ret = false;
    LOG("%s: %s\n", __func__, (true == ret ? "true" : "false"));
    return ret;
}

static inline bool doForceDisplayOnPostReboot() //Note: only to be used when the power state before reboot was ON.
{
    const char * flag_filename = "/opt/force_display_on_after_reboot";
    bool ret = false;
    if(0 == access(flag_filename, F_OK))
        ret = true;
    LOG("%s: %s\n", __func__, (true == ret ? "true" : "false"));
    return ret;
}

namespace pwrMgrProductTraits
{

/********************************* Begin base class definitions ********************************/
    bool ux_controller::set_bootloader_pattern(mfrBlPattern_t pattern)
    {
        mutex.lock();
        invalidateAsyncBootloaderPattern = true;
        mutex.unlock();
        return _set_bootloader_pattern(pattern);
    }

    bool ux_controller::_set_bootloader_pattern(mfrBlPattern_t pattern) const
    {
        bool ret = true;

        if (false == enableSilentRebootSupport)
            return true; // No-op on platforms that don't support it. Not logged was a warning/error as this is normal for many devices.

        IARM_Bus_MFRLib_SetBLPattern_Param_t mfrparam;
        mfrparam.pattern = pattern;
        if (IARM_RESULT_SUCCESS != IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetBootLoaderPattern, (void *)&mfrparam, sizeof(mfrparam)))
        {
            LOG("Warning! Call to IARM_BUS_MFRLIB_API_SetBootLoaderPattern failed.\n");
            ret = false;
        }
        else
        {
            LOG("%s: successfully set pattern %d\n", __func__, (int) pattern);
        }
        return ret;
    }
    void ux_controller::_set_bootloader_pattern_async(mfrBlPattern_t pattern) const
    {
        bool ret = true;
        const unsigned int retry_interval_seconds = 5;
        unsigned int remaining_retries = 12; //Give up after spending 1 minute trying to set pattern.
        LOG("%s start for pattern 0x%x\n", __func__, pattern);
        do
        {
            std::this_thread::sleep_for(std::chrono::seconds(retry_interval_seconds));
            std::unique_lock <std::mutex> lock (mutex);
            if(false == invalidateAsyncBootloaderPattern)
            {
                ret = _set_bootloader_pattern(pattern);
            }
            else
            {
                LOG("%s: bootloader pattern invalidated. Aborting.\n", __func__);
                break;
            }

        } while ((false == ret) && (0 < --remaining_retries));
        

        LOG("%s returns.\n", __func__);
    }
    
    bool ux_controller::set_bootloader_pattern_fault_tolerant(mfrBlPattern_t pattern) //Handy when you suspect that mfrmgr isn't up yet and may need retries to make this work.
    {
        /* Note: this function is only meant to be used once, and that too immediately after a reboot, when setting bootloader pattern for the first time. It is not meant to 
        be invoked more than once and is not safe for use in such a manner; it can be race prone. */ 
        bool ret = true;
        ret = _set_bootloader_pattern(pattern);
        if(false == ret)
        {
            //Failed to set bootloader pattern. This could be because mfrmgr isn't up yet. Fork a thread and retry repeatedly for a while.
            mutex.lock();
            invalidateAsyncBootloaderPattern = false;
            mutex.unlock();
            std::thread retry_thread(&ux_controller::_set_bootloader_pattern_async, this, pattern);
            retry_thread.detach();
        }
        return ret;
    }

    ux_controller::ux_controller(unsigned int in_id, const std::string &in_name, deviceType_t in_deviceType) : id(in_id), name(in_name), deviceType(in_deviceType)
    {
        LOG("%s: initializing for profile id %d, name %s\n", __func__, id, name.c_str());
        initialize_safe_defaults();
    }

    void ux_controller::initialize_safe_defaults()
    {
        enableMultiColourLedSupport = false;
        enableSilentRebootSupport = true;
        preferedPowerModeOnReboot = POWER_MODE_LAST_KNOWN;
        invalidateAsyncBootloaderPattern = false;
        firstPowerTransitionComplete = false;

        if (DEVICE_TYPE_STB == deviceType)
        {
            ledEnabledInStandby = false;
            ledEnabledInOnState = true;
        }
        else
        {
            ledEnabledInStandby = true;
            ledEnabledInOnState = true;
        }
    }

    void ux_controller::sync_power_led_with_power_state(IARM_Bus_PWRMgr_PowerState_t power_state) const
    {
        if (true == enableMultiColourLedSupport)
        {
            LOG("Warning! Device supports multi-colour LEDs but it isn't handled.");
        }

        bool led_state;
        if (IARM_BUS_PWRMGR_POWERSTATE_ON == power_state)
            led_state = ledEnabledInOnState;
        else
            led_state = ledEnabledInStandby;

        try
        {
            LOG("%s: setting power LED State to %s\n", __func__, (led_state ? "ON" : "FALSE"));
            device::FrontPanelIndicator::getInstance("Power").setState(led_state);
        }
        catch (...)
        {
            LOG("%s: Warning! exception caught when trying to change FP state\n", __func__);
        }
    }

    void ux_controller::sync_display_ports_with_power_state(IARM_Bus_PWRMgr_PowerState_t power_state) const
    {
        _SetAVPortsPowerState(power_state);
    }

    bool ux_controller::initialize_ux_controller(unsigned int profile_id) // Not thread-safe
    {
        bool ret = true;
        switch(profile_id)
        {
            case DEFAULT_STB_PROFILE:
                singleton = new ux_controller_stb(profile_id, "default-stb");
                break;
            
            case DEFAULT_TV_PROFILE:
                singleton = new ux_controller_tv(profile_id, "default-tv");
                break;
            
            case DEFAULT_STB_PROFILE_EUROPE:
                singleton = new ux_controller_stb_eu(profile_id, "default-stb-eu");
                break;

            case DEFAULT_TV_PROFILE_EUROPE:
                singleton = new ux_controller_tv_eu(profile_id, "default-tv-eu");
                break;
            
            default:
                LOG("Error! Unsupported product profile id %d\n", profile_id);
                ret = false;
        }
        return ret;
    }

    ux_controller * ux_controller::get_instance() //Not thread-safe.
    {
        return singleton;
    }
/********************************* End base class definitions ********************************/




/********************************* Begin ux_controller_tv_eu class definitions ********************************/


    ux_controller_tv_eu::ux_controller_tv_eu(unsigned int in_id, const std::string &in_name) : ux_controller(in_id, in_name, DEVICE_TYPE_TV)
    {
        preferedPowerModeOnReboot = POWER_MODE_LIGHT_SLEEP;
    }

    bool ux_controller_tv_eu::applyPowerStateChangeConfig(IARM_Bus_PWRMgr_PowerState_t new_state, IARM_Bus_PWRMgr_PowerState_t prev_state)
    {
        bool ret = true;
        sync_display_ports_with_power_state(new_state);
        sync_power_led_with_power_state(new_state);
        ret = set_bootloader_pattern((IARM_BUS_PWRMGR_POWERSTATE_ON == new_state ? mfrBL_PATTERN_NORMAL : mfrBL_PATTERN_SILENT_LED_ON));
        return ret;
    }

    bool ux_controller_tv_eu::applyPreRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state) const
    {
        return true;
    }
    bool ux_controller_tv_eu::applyPreMaintenanceRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state)
    {
        bool ret = true;
        if (IARM_BUS_PWRMGR_POWERSTATE_ON != current_state) // Silent reboot only applies if maintenance reboot is triggered while TV is in one of the standby states.
        {
            ret = set_bootloader_pattern(mfrBL_PATTERN_SILENT);
        }
        return ret;
    }

    bool ux_controller_tv_eu::applyPostRebootConfig(IARM_Bus_PWRMgr_PowerState_t target_state, IARM_Bus_PWRMgr_PowerState_t last_known_state /*last knnown power state from previous power cycle*/)
    {
        bool ret = true;
        /* Note: the product requires a special LED pattern that's set by bootloader/kernel as it boots. Since power manager
           doesn't support any fancy patterns yet, skip setting the LED configuration here so that we retain the boot pattern
           until app takes over. */
        if ((IARM_BUS_PWRMGR_POWERSTATE_ON == last_known_state) && (IARM_BUS_PWRMGR_POWERSTATE_STANDBY == target_state))
        {
            /* Special handling. Although the new power state is standby, leave display enabled. App will transition TV to ON state immediately afterwards anyway, 
               and if we turn off the display to match standby state here, it'll confuse the user into thinking that TV has gone into standby for good. */
            sync_display_ports_with_power_state(IARM_BUS_PWRMGR_POWERSTATE_ON);
        }
        else
        {
            sync_display_ports_with_power_state(target_state);
        }

        /*  Reset applicable 'silent reboot' flags to appropriate values.
            This device is booting into 'target_state' power state. Sync the bootloader flags to match the this state. That means, if the new state is ON,
            set up BL display and LED flags to match ON state. If the new state is STANDBY, set up BL and LED flags to match STANDBY state.
            */
        mfrBlPattern_t pattern = mfrBL_PATTERN_NORMAL;
        switch (target_state)
        {
            case IARM_BUS_PWRMGR_POWERSTATE_ON:
                //Do nothing. Pattern is already set to normal.
                break;

            case IARM_BUS_PWRMGR_POWERSTATE_STANDBY:             //deliberate fall-through
            case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP: //deliberate fall-through
            case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP:
                pattern = mfrBL_PATTERN_SILENT_LED_ON;
                break;
            default:
                LOG("%s: Warning! Unhandled power transition. New state: %d\n", __func__, target_state);
                break;
        }
        ret = set_bootloader_pattern_fault_tolerant(pattern);
        return ret;
    }

    IARM_Bus_PWRMgr_PowerState_t ux_controller_tv_eu::getPreferredPostRebootPowerState(IARM_Bus_PWRMgr_PowerState_t prev_state) const
    {
        return IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
    }

/********************************* End ux_controller_tv_eu class definitions ********************************/



/********************************* Begin ux_controller_stb_eu class definitions ********************************/
    ux_controller_stb_eu::ux_controller_stb_eu(unsigned int in_id, const std::string &in_name) : ux_controller(in_id, in_name, DEVICE_TYPE_STB)
    {
        preferedPowerModeOnReboot = POWER_MODE_LIGHT_SLEEP;
    }

    bool ux_controller_stb_eu::applyPowerStateChangeConfig(IARM_Bus_PWRMgr_PowerState_t new_state, IARM_Bus_PWRMgr_PowerState_t prev_state)
    {
        sync_display_ports_with_power_state(new_state);
        sync_power_led_with_power_state(new_state);        
        return true;
    }

    bool ux_controller_stb_eu::applyPreRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state) const
    {
        return true; //No-op
    }
    bool ux_controller_stb_eu::applyPreMaintenanceRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state)
    {
        bool ret = true;
        if (IARM_BUS_PWRMGR_POWERSTATE_ON != current_state) // Silent reboot only applies if maintenance reboot is triggered while STB is in one of the standby states.
        {
            ret = set_bootloader_pattern(mfrBL_PATTERN_SILENT);
        }
        return ret;
    }

    bool ux_controller_stb_eu::applyPostRebootConfig(IARM_Bus_PWRMgr_PowerState_t target_state, IARM_Bus_PWRMgr_PowerState_t last_known_state /*last knnown power state from previous power cycle*/)
    {
        bool ret = true;
        if ((IARM_BUS_PWRMGR_POWERSTATE_ON == last_known_state) && (IARM_BUS_PWRMGR_POWERSTATE_STANDBY == target_state))
        {
            //Special handling. Although the new power state is standby, leave display and LED enabled.
            sync_power_led_with_power_state(IARM_BUS_PWRMGR_POWERSTATE_ON);
            sync_display_ports_with_power_state(IARM_BUS_PWRMGR_POWERSTATE_ON);
        }
        else
        {
            sync_power_led_with_power_state(target_state);
            sync_display_ports_with_power_state(target_state);
        }

        /*  Reset applicable 'silent reboot' flags to appropriate values.*/
        ret = set_bootloader_pattern_fault_tolerant(mfrBL_PATTERN_NORMAL);
        return ret;
    }

    IARM_Bus_PWRMgr_PowerState_t ux_controller_stb_eu::getPreferredPostRebootPowerState(IARM_Bus_PWRMgr_PowerState_t prev_state) const
    {
        return IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
    }
/********************************* End ux_controller_stb_eu class definitions ********************************/



/********************************* Begin ux_controller_tv class definitions ********************************/
    ux_controller_tv::ux_controller_tv(unsigned int in_id, const std::string &in_name) : ux_controller(in_id, in_name, DEVICE_TYPE_TV)
    {
        preferedPowerModeOnReboot = POWER_MODE_LIGHT_SLEEP;
    }

    bool ux_controller_tv::applyPowerStateChangeConfig(IARM_Bus_PWRMgr_PowerState_t new_state, IARM_Bus_PWRMgr_PowerState_t prev_state)
    {
        mutex.lock();
        if(false == firstPowerTransitionComplete)
            firstPowerTransitionComplete = true; //In case we're still polling for reboot reason to turn on/off display, this will effectively cancel that job.
            //The new power state takes precedence.
        mutex.unlock();

        sync_display_ports_with_power_state(new_state);
        bool ret = set_bootloader_pattern((IARM_BUS_PWRMGR_POWERSTATE_ON == new_state ? mfrBL_PATTERN_NORMAL : mfrBL_PATTERN_SILENT_LED_ON));
        return ret;
    }

    bool ux_controller_tv::applyPreRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state) const
    {
        return true;
    }
    bool ux_controller_tv::applyPreMaintenanceRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state)
    {
        bool ret = true;
        if (IARM_BUS_PWRMGR_POWERSTATE_ON != current_state) // Silent reboot only applies if maintenance reboot is triggered while TV is in one of the standby states.
        {
            ret = set_bootloader_pattern(mfrBL_PATTERN_SILENT_LED_ON);
        }
        return ret;
    }

    bool ux_controller_tv::applyPostRebootConfig(IARM_Bus_PWRMgr_PowerState_t target_state, IARM_Bus_PWRMgr_PowerState_t last_known_state /*last knnown power state from previous power cycle*/)
    {
        bool ret = true;
        sync_power_led_with_power_state(target_state);

        if ((IARM_BUS_PWRMGR_POWERSTATE_ON == last_known_state) && (IARM_BUS_PWRMGR_POWERSTATE_STANDBY == target_state))
        {
            /* Special handling:
               Although the new power state is standby, leave display enabled. If last known power state is ON, app will transition TV to ON state
               immediately afterwards anyway, so if we turn off the display to match standby state here, it'll confuse the user into thinking that TV has gone into
               standby for good.
               
               An exception to this criteria is if we just got here after a hard reboot. In that case, the product requirement is to go straight to standby
               and stay there. Therefore we won't turn on the display here. This is to account for use cases where there is a power failure and power is restored
               when user is away - in that scenario, we assume that there is no audience and keep the TV in standby.

               Important: The above behaviour can be a nuisance during production or testing as it can lead to the TV going dark during various production/QA stages immediately
               after a hard reboot. Use doForceDisplayOnPostReboot() to detect those scenarios and act accordingly.
               */
            if(true == doForceDisplayOnPostReboot())
                sync_display_ports_with_power_state(IARM_BUS_PWRMGR_POWERSTATE_ON);
            else
            {
                reboot_type_t isHardReboot = getRebootType();
                switch (isHardReboot)
                {
                    case reboot_type_t::HARD:
                        sync_display_ports_with_power_state(IARM_BUS_PWRMGR_POWERSTATE_STANDBY);
                        break;

                    case reboot_type_t::SOFT:
                        sync_display_ports_with_power_state(IARM_BUS_PWRMGR_POWERSTATE_ON);
                        break;

                    default: //Unavailable. Take no action now, but keep checking every N seconds.
                        g_timeout_add_seconds(REBOOT_REASON_RETRY_INTERVAL_SECONDS, reboot_reason_cb, this);
                        break;
                }
            }
        }
        else
        {
            sync_display_ports_with_power_state(target_state);
        }

        /*  Reset applicable 'silent reboot' flags to appropriate values.
            This device is booting into 'target_state' power state. Sync the bootloader flags to match the this state. That means, if the new state is ON,
            set up BL display and LED flags to match ON state. If the new state is STANDBY, set up BL and LED flags to match STANDBY state.
            */
        mfrBlPattern_t pattern = mfrBL_PATTERN_NORMAL;
        switch (target_state)
        {
            case IARM_BUS_PWRMGR_POWERSTATE_ON:
                //Do nothing. Pattern is already set to normal.
                break;

            case IARM_BUS_PWRMGR_POWERSTATE_STANDBY:             //deliberate fall-through
            case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP: //deliberate fall-through
            case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP:
                pattern = mfrBL_PATTERN_SILENT_LED_ON;
                break;
            default:
                LOG("%s: Warning! Unhandled power transition. New state: %d\n", __func__, target_state);
                break;
        }
        ret = set_bootloader_pattern_fault_tolerant(pattern);
        return ret;
    }

    IARM_Bus_PWRMgr_PowerState_t ux_controller_tv::getPreferredPostRebootPowerState(IARM_Bus_PWRMgr_PowerState_t prev_state) const
    {
        return IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
    }

    void ux_controller_tv::sync_display_ports_with_reboot_reason(reboot_type_t reboot_type)
    {
        // Assumes the TV is rebooting from ON state to STANDBY state.
        mutex.lock();
        if(false == firstPowerTransitionComplete)
        {
            mutex.unlock();
            sync_display_ports_with_power_state(reboot_type_t::HARD == reboot_type? IARM_BUS_PWRMGR_POWERSTATE_STANDBY : IARM_BUS_PWRMGR_POWERSTATE_ON);
        }
        else // Do nothing, as we're already past the first power transition and display configuration has already been set as appropriate.
            mutex.unlock();
    }
/********************************* End ux_controller_tv class definitions ********************************/



/********************************* Begin ux_controller_stb class definitions ********************************/
    ux_controller_stb::ux_controller_stb(unsigned int in_id, const std::string &in_name) : ux_controller(in_id, in_name, DEVICE_TYPE_STB)
    {
        preferedPowerModeOnReboot = POWER_MODE_LAST_KNOWN;
        enableSilentRebootSupport = false;
    }

    bool ux_controller_stb::applyPowerStateChangeConfig(IARM_Bus_PWRMgr_PowerState_t new_state, IARM_Bus_PWRMgr_PowerState_t prev_state)
    {
        sync_display_ports_with_power_state(new_state);
        sync_power_led_with_power_state(new_state);        
        return true;
    }

    bool ux_controller_stb::applyPreRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state) const
    {
        return true; //No-op
    }
    bool ux_controller_stb::applyPreMaintenanceRebootConfig(IARM_Bus_PWRMgr_PowerState_t current_state)
    {
        bool ret = true;
        return ret;
    }

    bool ux_controller_stb::applyPostRebootConfig(IARM_Bus_PWRMgr_PowerState_t target_state, IARM_Bus_PWRMgr_PowerState_t last_known_state /*last knnown power state from previous power cycle*/)
    {
        bool ret = true;
        sync_power_led_with_power_state(target_state);
        sync_display_ports_with_power_state(target_state);
        return ret;
    }

    IARM_Bus_PWRMgr_PowerState_t ux_controller_stb::getPreferredPostRebootPowerState(IARM_Bus_PWRMgr_PowerState_t prev_state) const
    {
        return prev_state;
    }

/********************************* End ux_controller_stb class definitions ********************************/


}
