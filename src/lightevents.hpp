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

// Event Base declarations
// local events
ESP_EVENT_DECLARE_BASE(LCMD_EVENTS);        // declaration of the LightCommand events base
ESP_EVENT_DECLARE_BASE(LSTATE_EVENTS);      // declaration of the LightState events base
ESP_EVENT_DECLARE_BASE(LGROUP_EVENTS);      // declaration of the LightGroup events base
// remote events
ESP_EVENT_DECLARE_BASE(RCMD_EVENTS);        // declaration of the LightCommand events base
ESP_EVENT_DECLARE_BASE(RSTATE_EVENTS);      // declaration of the LightState events base
ESP_EVENT_DECLARE_BASE(RGROUP_EVENTS);      // declaration of the LightGroup events base

#define ID_ANONYMOUS    0
#define ID_BROADCAST    0xffff

#define NO_OVERRIDE     -1                  // Use light object's own setting

enum class light_event_id_t:int32_t {
    noop = 0,           // dummy event
    // CMD events
    echoRq,             // echo request
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
    getStatus,          // Get generic status info
    // State events
    echoRpl             // echo reply
};


/**
 * @brief Light Control Message structure
 * 
 */
struct Lcm {
    uint64_t src_uuid;
    uint64_t dst_uuid;
};

struct local_cmd_evt {
    //light_event_id_t evt;
    uint16_t src_id = ID_ANONYMOUS;
    uint16_t dst_id = ID_ANONYMOUS;
    uint32_t value = 0;
    int32_t step = NO_OVERRIDE;
    int32_t scale = NO_OVERRIDE;
    int32_t fade_duration = NO_OVERRIDE;
};

// Loop management
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
