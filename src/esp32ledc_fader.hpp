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
#include "esp32ledc.hpp"
#include "luma_curves.hpp"
#include <functional>


#define DEFAULT_FADE_TIME           1000           // ms

// a list of implemented fade engines
enum class fade_engine_t:uint8_t { none, linear_hw };

// a list of fade engine events
enum class fade_event_t:uint8_t {
    fade_start,
    fade_end
};

// fade controller callback type
typedef std::function<void (uint32_t channel, fade_event_t event)> fe_callback_t;

/**
 * @brief Abstract Null-Fader implementation algoritm
 * it does not fade anything, any specific implementation is meant for derived classes
 */
class FadeEngine {

protected:
    uint32_t channel;                               // PWMCtl channel to work on (not the esp32 channel)
    uint32_t fade_duration = DEFAULT_FADE_TIME;

public:
    const fade_engine_t engine;
    FadeEngine(uint32_t ch, fade_engine_t e = fade_engine_t::none) : channel(ch), engine(e) {};
    virtual ~FadeEngine(){};

    // Copy semantics : not (yet) implemented
    FadeEngine(const FadeEngine&) = delete;
    FadeEngine& operator=(const FadeEngine&) = delete;

    virtual bool fade(uint32_t duty, uint32_t duration) = 0;     // pure virtual method, must be redefined in derived classes

    //luma::curve luma_curve = luma::curve::linear;     // luma curve correction for relative methods (like xxPercent)
};

class FadeEngineHW : public FadeEngine {

protected:

public:
    // Derrived constructor
    FadeEngineHW(uint32_t ch) : FadeEngine(ch, fade_engine_t::linear_hw) { PWMCtl::getInstance()->chFadeISR(ch, true); };
    virtual ~FadeEngineHW(){};

    virtual bool fade(uint32_t duty, uint32_t duration) override;
};


struct ChannelFader {
    //bool state = 0;
    //luma::curve l_curve = luma::curve::linear;     // luma curve correction for relative methods (like xxPercent)
    FadeEngine *fe = nullptr;
    fe_callback_t cb = nullptr;
};

// Fade controller class
class FadeCtrl {

    // channel faders
    ChannelFader chf[LEDC_SPEED_MODE_MAX*LEDC_CHANNEL_MAX];

    PWMCtl *pwm;
    uint32_t events_mask;                        // bit mask for channel event group
    TaskHandle_t t_fade_evt = nullptr;           // fade events handler
    EventGroupHandle_t *eg_fade_evt = nullptr;

    void fd_events_handler();

    // static wrapper for event handler Task
    static void evtTask(void* pvParams){
        ((FadeCtrl*)pvParams)->fd_events_handler();
    }

    bool nofade(uint8_t ch, uint32_t duty);

    //bool fadebyTime(uint8_t ch, uint32_t duty, uint32_t duration);

public:
    FadeCtrl( uint32_t mask = CH_EVENTS_BIT_MASK );
    ~FadeCtrl();    // todo: destruct faders


    bool setFader(uint8_t ch, fade_engine_t fe, fe_callback_t f = nullptr );
    bool fadebyTime(uint8_t ch, uint32_t duty, uint32_t duration);

    //void setCurve(uint8_t ch, luma::curve curve){ ch %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX; chf[ch].l_curve = curve; }

    //inline virtual uint32_t setFadeDuration(uint32_t duration){ fade_duration = duration; return fade_duration; }
    //inline virtual uint32_t getFadeDuration() const { return fade_duration; }

};

