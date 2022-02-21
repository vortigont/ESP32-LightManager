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
#include "light_generics.hpp"
#include "LList.h"



// Light objects

/**
 * @brief ECLO - Event Controlled Light Object
 * is a light object based on GenericLight or any of it's derivatives attached to
 * control event loop. It is controlled via event messaged
 * 
 */
class Eclo {

    struct Evt_subscription {
        esp_event_base_t base;
        int32_t event_id;
        esp_event_handler_instance_t evt_instance;
    };


    std::shared_ptr<GenericLight> light;
    uint16_t id;
    std::unique_ptr<char[]> descr;                          // Mnemonic name for the instance
    LList<Evt_subscription> subscr;                         // list of event subscriptions

    void unsubscribe();

//protected:
    /**
     * @brief static event handler
     * wraps class members access for event loop
     * 
     * @param handler_args 
     * @param base 
     * @param id 
     * @param event_data 
     */
    static void event_hndlr(void* handler_args, esp_event_base_t base, int32_t rcpt, void* event_data);

    /**
     * @brief event picker method, processes incoming events from a event_hndlr wrapper
     * 
     * @param base 
     * @param id 
     * @param event_data 
     */
    void event_picker(esp_event_base_t base, int32_t evid, void* event_data);

    /**
     * @brief process 'Command base' events
     * events meant to handled as commands for this light object
     * 
     * @param base 
     * @param id 
     * @param event_data 
     */
    void evt_cmd_runner(esp_event_base_t base, int32_t evid, local_cmd_evt const *cmd);


    void evt_state_post(light_event_id_t evnt = light_event_id_t::stateUpdate, int32_t groupid = ID_ANONYMOUS, uint16_t dst = ID_ANONYMOUS);

public:
    Eclo(GenericLight *l, uint16_t id, const char *_descr=nullptr);
    ~Eclo();


    /**
     * @brief subscribe to events loop with specified base:id event identifiers
     * 
     * @param base ESP event base
     * @param id event id
     * @return true on success
     * @return false on error
     */
    bool evt_subscribe(esp_event_base_t base, int32_t id);


};