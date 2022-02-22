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

// fwd declare
class Eclo;

// event loop message callback type
typedef std::function<void (Eclo* lo, esp_event_base_t base, int32_t evid, void* data)> event_loop_cb_t;

// Light objects

/**
 * @brief ECLO - Event Controlled Light Object
 * is a light object based on GenericLight or any of it's derivatives attached to
 * control event loop. It is controlled via event messaged loop only and to the same
 * loop it pushes it's state updates
 * 
 */
class Eclo {

    struct Evt_subscription {
        esp_event_base_t base;
        int32_t event_id;
        esp_event_handler_instance_t evt_instance;
    };


    std::shared_ptr<GenericLight> light;
    std::unique_ptr<char[]> descr;                          // Mnemonic name for the instance
    LList<Evt_subscription> subscr;                         // list of event subscriptions
    event_loop_cb_t unknown_evnt_cb = nullptr;              // external callback for unknown events

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

    /**
     * @brief post event message with light state
     * default is post to anonymous group
     * 
     * @param evnt - event type, report or on-update
     * @param groupid - group to post to
     * @param dst - recipiet's id
     */
    void evt_state_post(light_event_id_t evnt = light_event_id_t::stateUpdate, int32_t groupid = ID_ANONYMOUS, uint16_t dst = ID_ANONYMOUS);

    /**
     * @brief post an event message - reply to ping
     * 
     * @param evnt 
     * @param groupid 
     * @param dst 
     */
    void evt_pong_post(int32_t groupid, uint16_t dst);

public:

    uint16_t const id;      // object ID to be used in event control messages

    /**
     * @brief Construct a new Eclo object
     * a source GenericLight pointer will be invalidated on creation and can't be used any more
     * To get the pointer to the object use getLight() method
     * @param l - GenericLight object (or it's derivatives) to take control
     * @param id - any random identificator except 0(anonymous) and 65535(broadcast)
     * @param _descr - mnemonic description
     */
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

    /**
     * @brief Get shred pointer to the Light object
     * 
     * @return std::shared_ptr<GenericLight> 
     */
    std::shared_ptr<GenericLight> getLight(){return light;};

    /**
     * @brief unsubscribe from event loop all types of events
     * 
     */
    void unsubscribe();

    /**
     * @brief attach external event loop callback function
     * all uknown events will be redirected there
     * to detach callbach - pass a nullptr here
     * @param f callback function prototype: event_loop_cb_t
     */
    void eventcbAttach(event_loop_cb_t f);

};


