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

#include "lightmanager.hpp"
#include <string.h>

// LOGGING
#ifdef ARDUINO
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

static const char* TAG = "light_mgr";


// Classes implementation
Eclo::Eclo(GenericLight *l, uint16_t id, const char *_descr) : id(id) {
        if (!_descr || !*_descr){
            descr.reset(new char[12]);   // i.e. eclo-12345
            sprintf(descr.get(), "eclo-%d", id);
        }  else
            descr.reset(strcpy(new char[strlen(_descr) + 1], _descr));


    light.reset(std::move(l));                              // relocate GenericLight object

    evt_subscribe(LCMD_EVENTS, ESP_EVENT_ANY_ID);           // subscribe to localy originated command events
    //evt_subscribe(RCMD_EVENTS, mk_uuid(id));              // subscribe to remotely originated events
}

Eclo::~Eclo(){
    unsubscribe();
}

void Eclo::event_hndlr(void* handler_args, esp_event_base_t base, int32_t evid, void* event_data){
    ESP_LOGI(TAG, "eclo event handling %s:%d", base, evid);
    reinterpret_cast<Eclo*>(handler_args)->event_picker(base, static_cast<light_event_id_t>(evid), event_data);
}


bool Eclo::evt_subscribe(esp_event_base_t base, int32_t id){
    esp_event_handler_instance_t evt_instance;

    esp_err_t err = esp_event_handler_instance_register_with(*get_light_evts_loop(), ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, Eclo::event_hndlr, this, &evt_instance);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE){
    	ESP_LOGW(TAG, "event loop subscribe failed for %s", descr.get());
        return false;
    }

  	ESP_LOGI(TAG, "event loop subscribed %s %s:%d", descr.get(), base, id);
    //auto event_instance = std::make_shared<Evt_subscription>();
    Evt_subscription event_instance;
    event_instance.base = base;
    event_instance.event_id = id;
    event_instance.evt_instance = evt_instance;

    return subscr.add(event_instance);
}

void Eclo::event_picker(esp_event_base_t base, light_event_id_t evid, void* event_data){
    ESP_LOGI(TAG, "%s event picker %s:%d", descr.get(), base, evid);

    if (base == LCMD_EVENTS){
        local_cmd_evt *cmd = reinterpret_cast<local_cmd_evt*>(event_data);

        if(cmd->dst_id == id)       // respond to commands addressed to my id
            return evt_cmd_runner(base, evid, cmd);
    }

    // TODO: remote/group events logic, etc...
}

void Eclo::unsubscribe(){
    auto loop = get_light_evts_loop();
    if (!loop)
        return;

    while(subscr.size()){
        auto node = subscr.pop();
        esp_event_handler_instance_unregister_with(*loop, node.base, node.event_id, node.evt_instance);
    }
}

void Eclo::evt_cmd_runner(esp_event_base_t base, light_event_id_t evid, local_cmd_evt const *cmd){

    switch(evid){
        case light_event_id_t::goValue :
            return light->goValue(cmd->value, cmd->fade_duration);
        case light_event_id_t::goValueScaled :
            return light->goValueScaled(cmd->value, cmd->scale, cmd->fade_duration);
        case light_event_id_t::goMax :
            return light->goMax(cmd->fade_duration);
        case light_event_id_t::goMin :
            return light->goMin(cmd->fade_duration);
        case light_event_id_t::goOn :
            return light->goOn(cmd->fade_duration);
        case light_event_id_t::goOff :
            return light->goOff(cmd->fade_duration);
        case light_event_id_t::goToggle :
            return light->goToggle(cmd->fade_duration);
        case light_event_id_t::goIncr :
            return light->goIncr(cmd->fade_duration);
        case light_event_id_t::goDecr :
            return light->goDecr(cmd->fade_duration);
        case light_event_id_t::goStep :
            return light->goStep(cmd->step, cmd->fade_duration);
        case light_event_id_t::goStepScaled :
            return light->goStepScaled(cmd->step, cmd->scale, cmd->fade_duration);

        default :
            break;
        //getStatus
        //echoRq
    }
    //light->goToggle();

}
