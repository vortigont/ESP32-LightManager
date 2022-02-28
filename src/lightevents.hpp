/*
    ESP32 Light Manager library

This code implements a library for ESP32-xx family chips and provides an
API for controling Lighting applications, mostly (but not limited to) LEDs,
LED strips, PWM drivers, RGB LED strips etc...


Copyright (C) Emil Muratov, 2022
GitHub: https://github.com/vortigont/ESP32-LightManager

 *  This program or library is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General
 *  Public License along with this library; if not, get one at
 *  https://opensource.org/licenses/LGPL-3.0
*/

#pragma once

extern "C" {
#include "esp_event.h"
#include "esp_system.h"
}   //extern "C"

#include "light_types.hpp"
#include <bitset>

// Event Base declarations
// local events
ESP_EVENT_DECLARE_BASE(LCMD_EVENTS);        // declaration of the local Light Command events base
ESP_EVENT_DECLARE_BASE(LSTATE_EVENTS);      // declaration of the local Light State events base
ESP_EVENT_DECLARE_BASE(LSERVICE_EVENTS);    // declaration of the local Light Service events base
// remote events
ESP_EVENT_DECLARE_BASE(RCMD_EVENTS);        // declaration of the remote LightCommand events base
ESP_EVENT_DECLARE_BASE(RSTATE_EVENTS);      // declaration of the remote LightState events base
ESP_EVENT_DECLARE_BASE(RSERVICE_EVENTS);    // declaration of the remote Light Service events base

#define ID_ANONYMOUS    0
#define ID_ANY          0xffff
#define GROUP_SELF      0
#define GROUP_ANY       ESP_EVENT_ANY_ID    // ESP_EVENT_ANY_ID     -1
#define NO_OVERRIDE     -1                  // Use light object's own setting


// group permissions
#define GRP_BIT_R     0
#define GRP_BIT_W     1
#define GRP_BIT_LEN   2

enum class grp_perms_t:uint8_t {
    read = 1,
    write,
    rw
};

enum class light_event_id_t:uint8_t {
    noop = 0,           // dummy event
    // Light Command Events
    lce_start,          // begin marker
    goValue,            // set basic brightness level
    goValueScaled,
    goMax,
    goMin,
    goOn,
    goOff,
    goToggle,
    goIncr,
    goDecr,
    goStep,
    goStepScaled,
    lce_end,            // end marker
    // Light State Events
    lse_start,
    stateReport,
    stateUpdate,
    lse_end,
    // Service events
    se_start,
    echoRq,             // echo request
    echoRpl,            // echo reply
    getState,           // Get generic status info
    se_end
};


/**
 * @brief local event source and destination ids
 * 
 */
struct local_peers_id_t {
    uint16_t src;
    uint16_t dst;
};

/**
 * @brief Local service event data structure
 * a generic data carrier for service events
 */
struct local_srvc_evt {
    light_event_id_t event;
    local_peers_id_t id;
    uint32_t value;         // abstract data field
};


/**
 * @brief Local control command event data
 * a generic data carrier for cmommand event within one system
 * suitable for sending events CMD events light_event_id_t
 * (i.e. go*)
 */
struct local_cmd_evt {
    light_event_id_t event;
    local_peers_id_t id;
    uint32_t value = 0;
    int32_t step = NO_OVERRIDE;
    int32_t scale = NO_OVERRIDE;
    int32_t fade_duration = NO_OVERRIDE;
};


struct local_state_evt {
    light_event_id_t event;
    local_peers_id_t id;
    light_state_t state;
};

/**
 * @brief event loop subscription
 * describe event subscription for an object
 * and permissions for the group
 * 
 */
struct Evt_subscription {
    esp_event_base_t base;
    int32_t gid;
    std::bitset<GRP_BIT_LEN> grpmode;
    esp_event_handler_instance_t evt_instance;
};


// Loop management


namespace lightmgr {
/**
 * @brief Starts Light event loop task
 * this loop will manage events processing between light components and controls
 * 
 * @return esp_event_loop_handle_t* a pointer to loop handle
 */
esp_event_loop_handle_t* start_levt_loop();

/**
 * @brief Get the pointer to levt_loop handler
 * 
 * @return esp_event_loop_handle_t* loop handler pointer
 */
esp_event_loop_handle_t* get_light_evts_loop();

/**
 * @brief Generate uuid for this system based on provided 16 bit id
 * UUID is 64 bit long: 48 bit MAC + 16 bit id
 * 48 bit of base MAC address factory-programmed by Espressif in BLK0 of EFUSE
 * 
 * @param id 
 * @return uint64_t 
 */
inline uint64_t mk_uuid(uint16_t id);

void event_state_printer(esp_event_base_t base, int32_t gid, local_state_evt const *data);

}   // namespace lightmgr