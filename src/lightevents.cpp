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

#include "lightevents.hpp"
// LOGGING
#ifdef ARDUINO
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

extern "C" {
#include "esp_system.h"
}   //extern "C"


static const char* TAG = "light_evt";

#define LOOP_LEVT_Q_SIZE        32              // events loop queue size
#define LOOP_LEVT_T_PRIORITY    2               // task priority is a bit higher that arduino's loop()
#define LOOP_LEVT_T_STACK_SIZE  4096            // task stack size


// Event Base definitions
ESP_EVENT_DEFINE_BASE(LCMD_EVENTS);
ESP_EVENT_DEFINE_BASE(LSTATE_EVENTS);
ESP_EVENT_DEFINE_BASE(LSERVICE_EVENTS);
ESP_EVENT_DEFINE_BASE(RCMD_EVENTS);
ESP_EVENT_DEFINE_BASE(RSTATE_EVENTS);
ESP_EVENT_DEFINE_BASE(RSERVICE_EVENTS);

// LighEvents loop handler
static esp_event_loop_handle_t loop_levt_h = nullptr;


// Implementations
namespace lightmgr {


esp_event_loop_handle_t* start_levt_loop(){
    if (loop_levt_h)
        return &loop_levt_h;

    ESP_LOGI(TAG, "loop set up");

    esp_event_loop_args_t levt_cfg;
    levt_cfg.queue_size = LOOP_LEVT_Q_SIZE;
    levt_cfg.task_name = "evtloop_t";
    levt_cfg.task_priority = LOOP_LEVT_T_PRIORITY;          // uxTaskPriorityGet(NULL) // same as parent
    levt_cfg.task_stack_size = LOOP_LEVT_T_STACK_SIZE;
    levt_cfg.task_core_id = tskNO_AFFINITY;


    //ESP_ERROR_CHECK(esp_event_loop_create(&levt_cfg, &loop_levt_h));
    esp_err_t err = esp_event_loop_create(&levt_cfg, &loop_levt_h);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    	ESP_LOGE(TAG, "create event loop evtloop_t failed!");
    }

    return &loop_levt_h;
}

esp_event_loop_handle_t* get_light_evts_loop(){
    if (!loop_levt_h)
        start_levt_loop();

    return &loop_levt_h;
}

uint64_t mk_uuid(uint16_t id){
    uint64_t uuid;
    esp_efuse_mac_get_default((uint8_t*)uuid);
    uuid <<= 16;
    return uuid |= id;
};


void event_state_printer(esp_event_base_t base, int32_t gid, local_state_evt const *data){
    if (base != LSTATE_EVENTS){
        printf("=== not an LSTATE_EVENTS event ===\n");
        return;
    }

    printf("=== LSTATE_EVENTS event ===\n");

    printf("MSG Group id:\t%d\n", gid);
    printf("MSG address src_id: %d,\tdst_id: %d\n", data->id.src, data->id.dst);
    printf("Light object state:\n");
    printf("lightsource type: %d\nluma curve: %d\n", (uint8_t)data->state.ltype, (uint8_t)data->state.luma);
    printf("fade time: %d,\nIncrement step: %d\n", data->state.fadetime, data->state.increment);
    printf("Active logic level:\t%s\n", data->state.active_ll ? "H" : "L");
    printf("Brighness value:\t%d/%d\n", data->state.value, data->state.value_max);
    printf("Brighness scaled:\t%d/%d\n", data->state.value_scaled, data->state.brtscale);
    printf("Power value: %.2f out of %.2f\n", data->state.power, data->state.power_max);

}


}