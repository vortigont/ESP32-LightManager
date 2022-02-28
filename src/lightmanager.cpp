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
Eclo::Eclo(GenericLight *l, uint16_t id, const char *_descr) : myid(id) {
        if (!_descr || !*_descr){
            descr.reset(new char[12]);   // i.e. eclo-12345
            sprintf(descr.get(), "eclo-%d", id);
        }  else
            descr.reset(strcpy(new char[strlen(_descr) + 1], _descr));

    light.reset(std::move(l));                              // relocate GenericLight object

    grp_subscribe(myid, grp_perms_t::rw);                   // subscribe to local private group matching myid (default one)

    /*
     * Attach to onChange() light object handler
     * this lamda will post stateUpdate event to the loop on any light change
     * to all registered groups with WRITE permission
     */
    light->onChangeAttach([this](){
        for (auto i : subscr){
            if (i.base != LSTATE_EVENTS || !i.grpmode.test(GRP_BIT_W))     // skip non-writable groups
                continue;

            evt_state_post(light_event_id_t::stateUpdate, i.gid, ID_ANONYMOUS);
        }
    });
}

Eclo::~Eclo(){
    unsubscribe();
}

void Eclo::event_hndlr(void* handler_args, esp_event_base_t base, int32_t gid, void* event_data){
    ESP_LOGD(TAG, "eclo event handling %s:%d", base, gid);
    reinterpret_cast<Eclo*>(handler_args)->event_picker(base, gid, event_data);
}

bool Eclo::evt_subscribe(esp_event_base_t base, int32_t gid, grp_perms_t perm){

    // check if such base:gid has been registered already
    for (auto i : subscr){
        if (i.base == base && i.gid == gid){
        	ESP_LOGW(TAG, "%s: already subscribed for %s:%d", descr.get(), base, gid);
            return false;
        }
    }

    esp_event_handler_instance_t evt_instance;

    esp_err_t err = esp_event_handler_instance_register_with(*get_light_evts_loop(), base, myid, Eclo::event_hndlr, this, &evt_instance);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE){
    	ESP_LOGW(TAG, "%s: event loop subscribe failed for %s:%d", descr.get(), base, gid);
        return false;
    }

    Evt_subscription event_instance = {
        base,
        gid,
        static_cast<uint8_t>(perm),
        std::move(evt_instance)
    };

  	ESP_LOGI(TAG, "%s: event loop subscribed to %s:%d", descr.get(), base, gid);
    return subscr.add(std::move(event_instance));
}

void Eclo::event_picker(esp_event_base_t base, int32_t gid, void* event_data){
    ESP_LOGI(TAG, "%s event picker %s:%d", descr.get(), base, gid);

    if (base == LCMD_EVENTS){
        // check if this group has permission to receive control messages
        const Evt_subscription *sub = subscr_by_gid(gid);
        if (!sub){
            ESP_LOGW(TAG, "%s unregistered event group %s:%d", descr.get(), base, gid);
            return;
        }

        if (sub->grpmode.test(GRP_BIT_R)){
            // todo: check that *data is actually local_cmd_evt
            local_cmd_evt *cmd = reinterpret_cast<local_cmd_evt*>(event_data);
            evt_cmd_runner(base, gid, cmd);
            //return;
        }
        return;     // ?? не выходить а сбрасывать обработку на внешний коллбек
    }

    if (base == LSERVICE_EVENTS){
        local_srvc_evt *e = reinterpret_cast<local_srvc_evt*>(event_data);

        if (e->id.dst != myid || e->id.dst != ID_ANY)       // ignore messages not to me or not broadcast
            return;

        switch(e->event){
            case light_event_id_t::echoRq :                                                // do echo reply
                return evt_pong_post(gid, e->id.src);
            case light_event_id_t::getState :
                return evt_state_post(light_event_id_t::stateReport, gid, e->id.src);      // status report
            default :
                return;
        }
    }

    // TODO: remote/group events logic, etc...

    /**
     * if we get here, than some unknown event occured,
     * it can be handled via external callback function set by user
     */
    if (unknown_evnt_cb)
        unknown_evnt_cb(this, base, gid, event_data);
}

void Eclo::unsubscribe(){
    auto loop = get_light_evts_loop();
    if (!loop)
        return;

    while(subscr.size()){
        auto node = subscr.pop();
        esp_event_handler_instance_unregister_with(*loop, node.base, node.gid, node.evt_instance);
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
    local_state_evt st = {
        evnt,
        { myid, dst },    // src, dst id
        light->getState()
    };

    ESP_ERROR_CHECK(esp_event_post_to( *get_light_evts_loop(), LSTATE_EVENTS, groupid ? groupid : myid, &st, sizeof(local_state_evt), 100 / portTICK_PERIOD_MS));
}

void Eclo::evt_pong_post(int32_t groupid, uint16_t dst){
    local_srvc_evt msg = {
        light_event_id_t::echoRpl,  // event type
        { myid, dst },                // msg addtess id
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

Evt_subscription const *Eclo::subscr_by_gid(uint16_t gid) const {
    for (auto i = subscr.cbegin(); i != subscr.cend(); ++i){
        if (i->gid == gid)
            return i.operator->();
        //    return i
    }
    return nullptr;
}

bool Eclo::grp_subscribe(int32_t gid, grp_perms_t perm){
    // not nice
    evt_subscribe(LCMD_EVENTS, gid, perm);                  // subscribe to local gid command events
    return evt_subscribe(LSERVICE_EVENTS, gid, perm);              // subscribe to local gid service events
}
