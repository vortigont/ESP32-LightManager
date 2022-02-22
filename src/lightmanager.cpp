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

using namespace lightmgr;

// Classes implementation
Eclo::Eclo(GenericLight *l, uint16_t id, const char *_descr) : id(id) {
        if (!_descr || !*_descr){
            descr.reset(new char[12]);   // i.e. eclo-12345
            sprintf(descr.get(), "eclo-%d", id);
        }  else
            descr.reset(strcpy(new char[strlen(_descr) + 1], _descr));

    light.reset(std::move(l));                              // relocate GenericLight object

    evt_subscribe(LCMD_EVENTS, id);                         // subscribe to localy originated command events destined to MY group id
    evt_subscribe(LSERVICE_EVENTS, id);                     // subscribe to localy originated service events destined to MY group id
    evt_subscribe(LSERVICE_EVENTS, ID_BROADCAST);           // subscribe to localy originated service events destined to broadcast group id
    //evt_subscribe(LCMD_EVENTS, ESP_EVENT_ANY_ID);         // subscribe to ALL localy originated command events
    //evt_subscribe(RCMD_EVENTS, mk_uuid(id));              // subscribe to remotely originated events

    light->onChangeAttach([this](){ evt_state_post(); });   // send stateUpdate events on light change
}

Eclo::~Eclo(){
    unsubscribe();
}

void Eclo::event_hndlr(void* handler_args, esp_event_base_t base, int32_t rcpt, void* event_data){
    ESP_LOGD(TAG, "eclo event handling %s:%d", base, rcpt);
    reinterpret_cast<Eclo*>(handler_args)->event_picker(base, rcpt, event_data);
}


bool Eclo::evt_subscribe(esp_event_base_t base, int32_t id){
    esp_event_handler_instance_t evt_instance;

    // TODO: добавить проверку что подписка на такое событие уже существует
    esp_err_t err = esp_event_handler_instance_register_with(*get_light_evts_loop(), base, id, Eclo::event_hndlr, this, &evt_instance);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE){
    	ESP_LOGW(TAG, "event loop subscribe failed for %s", descr.get());
        return false;
    }

  	ESP_LOGI(TAG, "%s: event loop subscribed to %s:%d", descr.get(), base, id);
    //auto event_instance = std::make_shared<Evt_subscription>();
    Evt_subscription event_instance;
    event_instance.base = base;
    event_instance.event_id = id;
    event_instance.evt_instance = evt_instance;

    return subscr.add(std::move(event_instance));
}

void Eclo::event_picker(esp_event_base_t base, int32_t rcpt, void* event_data){
    ESP_LOGI(TAG, "%s event picker %s:%d", descr.get(), base, rcpt);

    if (base == LCMD_EVENTS){
        local_cmd_evt *cmd = reinterpret_cast<local_cmd_evt*>(event_data);
        return evt_cmd_runner(base, rcpt, cmd);
    }

    if (base == LSERVICE_EVENTS){
        local_srvc_evt *e = reinterpret_cast<local_srvc_evt*>(event_data);
        switch(e->event){
            case light_event_id_t::echoRq :                                                 // echo reply
                return evt_pong_post(rcpt, e->id.src);
            case light_event_id_t::getState :
                return evt_state_post(light_event_id_t::stateReport, rcpt, e->id.src);      // status report
            default :
                break;
        }
    }

    // TODO: remote/group events logic, etc...

    /**
     * if we get here, than some unknown event occured,
     * it can be handled via external callback function set by user
     */
    if (unknown_evnt_cb)
        unknown_evnt_cb(this, base, rcpt, event_data);
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

void Eclo::evt_cmd_runner(esp_event_base_t base, int32_t rcpt, local_cmd_evt const *cmd){

    switch(cmd->event){
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

}

void Eclo::evt_state_post(light_event_id_t evnt, int32_t groupid, uint16_t dst){
    if (groupid == ID_BROADCAST)    // do not reply to broadcast, reply to dst group
        groupid = dst;

    local_peers_id_t addr;
    addr.src = id;
    addr.dst = dst;

    local_state_evt st = {
        evnt,
        addr,
        std::move(light->getState())
    };

    //light_state_t state = std::move(light->getState());
    ESP_ERROR_CHECK(esp_event_post_to( *get_light_evts_loop(), LSTATE_EVENTS, groupid, &st, sizeof(local_state_evt), 100 / portTICK_PERIOD_MS));
}

void Eclo::evt_pong_post(int32_t groupid, uint16_t dst){
    if (groupid == ID_BROADCAST)    // do not reply to broadcast, reply to dst group
        groupid = dst;

    local_srvc_evt msg = {
        light_event_id_t::echoRpl,  // event type
        { id, dst },                // msg addtess id
        0                           // custom value
    };

    ESP_ERROR_CHECK(esp_event_post_to( *get_light_evts_loop(), LSTATE_EVENTS, groupid, &msg, sizeof(local_srvc_evt), 100 / portTICK_PERIOD_MS));
}

void Eclo::eventcbAttach(event_loop_cb_t f){
    if (f)
        unknown_evnt_cb = std::move(f);
    else
        unknown_evnt_cb = nullptr;
}
