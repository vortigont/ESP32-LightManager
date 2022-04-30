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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/ledc.h"

#define DEFAULT_PWM_FREQ            2000
#define DEFAULT_PWM_RESOLUTION      LEDC_TIMER_10_BIT
#define DEFAULT_PWM_CLK             LEDC_AUTO_CLK
#define DEFAULT_PWM_DUTY            0                   //  (1<<DEFAULT_PWM_RESOLUTION - 1)     // 50% duty
#define DEFAULT_MAX_DUTY            ((1<<DEFAULT_PWM_RESOLUTION) - 1)

#if configUSE_16_BIT_TICKS
#define MAX_EG_BITS                 8
#else
#define MAX_EG_BITS                 24
#endif

#define CH_EVENTS_BIT_MASK          ((1 << LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX) - 1)

// macro's
#define BIT_SET(var, bit) (var |= 1<<bit)
#define BIT_CLR(var, bit) (var &= ~(1<<bit))
#define BIT_READ(var, bit) ((var >> bit) & 1)

/*
refs:
    Table 69: Commonly­ used Frequencies and Resolutions
    RTC8M_CLK (8 MHz) 8 kHz 1/512 (9 bit)
*/

// need this until https://github.com/espressif/esp-idf/pull/8247
uint32_t wrap_ledc_get_max_duty(ledc_mode_t speed_mode, ledc_channel_t channel);

enum class ch_state:uint8_t { stop, active };
enum class tm_state:uint8_t { stop, active, pause };

/**
 * @brief in contrast with ESP-IDF this enum ALWAYS matches '0' with LOW_SPEED
 * and '1' with HIGH_SPEED.
 * In ESP-IDF this is all so weird:
 * original esp32 '0' is a HIGH_SPEED
 * derivatives (like esp32-c3, esp32-s2, etc) '0' is the LOW_SPEED
 * Those guys must be smoking something? :)
 */
enum class realspeedmode_t:uint8_t { low, high };

namespace ledc {

struct timer {
    ledc_timer_config_t cfg;
    tm_state state = tm_state::stop;
    realspeedmode_t getRealSpeedMode() const {
#if SOC_LEDC_SUPPORT_HS_MODE
        return cfg.speed_mode ? realspeedmode_t::low : realspeedmode_t::high;
#else
        return realspeedmode_t::low;
#endif
    }
};

struct ch {
    ledc_channel_config_t cfg;
    ch_state state = ch_state::stop;
    //bool initialized = false;
    bool idle_level = 0;
    bool fade_cb = false;
    realspeedmode_t getRealSpeedMode() const {
#if SOC_LEDC_SUPPORT_HS_MODE
        return cfg.speed_mode ? realspeedmode_t::low : realspeedmode_t::high;
#else
        return realspeedmode_t::low;
#endif
    }
};

/*
class PWMTimers {
    ledc_timer timers[LEDC_SPEED_MODE_MAX*LEDC_TIMER_MAX];
public:
    PWMTimers();

    ledc_timer const *getTm(uint8_t t) const { return &timers[t % (LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX)]; };
    int init(uint8_t tm);
};

class PWMChannels{
    ledc_ch channels[LEDC_SPEED_MODE_MAX*LEDC_CHANNEL_MAX];

public:
    PWMChannels();   

    int init(uint32_t ch, int pin = -1);

    ledc_ch const *getCh(uint32_t ch) const { return &channels[ch % (LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX)]; };

    int setPin(uint32_t ch, int pin, bool idlelvl = false, bool invert = false);
//    bool ch_set_pin(int pin, uint32_t ch, bool hispeed);

    int attachTimer(uint32_t ch, uint8_t timer);

    int setDuty(uint32_t ch, uint32_t duty, uint32_t phase = 0);

    void fade(uint32_t ch, bool dir);
};
*/

} // namespace ledc


class PWMCtl {

    static EventGroupHandle_t g_fade_evt;
    bool faderIRQ = false;     // fader interrupt installed

public:
    // this is a singleton
    PWMCtl(PWMCtl const&) = delete;
    void operator=(PWMCtl const&) = delete;

    /**
     * obtain a pointer to singleton instance
     */
    static PWMCtl *getInstance(){
        static PWMCtl instance;
        return &instance;
    }


    // Channel methods
    int chStart(uint32_t ch, int pin = -1);
    int chStop(uint32_t ch);
    int chSet(uint32_t ch, int pin, bool idlelvl = false, bool invert = false);
    int chFadeISR(uint32_t ch, bool enable);
    int chAttachTimer(uint32_t ch, uint8_t timer);

    esp_err_t chDuty(uint32_t ch, uint32_t duty);
    esp_err_t chPhase(uint32_t ch, uint32_t phase);
    esp_err_t chDutyPhase(uint32_t ch, uint32_t duty, uint32_t phase);

    uint32_t chGetDuty(uint32_t ch) const;

    /**
     * @brief get Duty-Offset (phase) for a channel
     * 
     * @return uint32_t Duty offset (0 - MAX_DUTY)
     */
    uint32_t chGetPhase(uint32_t ch) const;

    uint32_t chGetMaxDuty(uint32_t ch) const;   // { CH_SAFE(ch); return wrap_ledc_get_max_duty(channels[ch].cfg.speed_mode, channels[ch].cfg.channel); };
    ledc::ch const *chGet(uint32_t ch) const { return &channels[ch % (LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX)]; };

    /**
     * @brief find timer attached to a specific channel
     * 
     * @param ch channel number
     * @return uint32_t timer number attached
     */
    uint8_t chGetTimernum(int32_t ch) const;


    // Timer methods
    int tmStart(uint8_t tm);
    esp_err_t tmSet(uint8_t tm, ledc_timer_bit_t bits, uint32_t hz);
    esp_err_t tmSetFreq(uint8_t tm, uint32_t hz);
    uint32_t tmGetFreq(uint8_t tm) const;

    EventGroupHandle_t *getFaderEventGroup();

private:
    PWMCtl();   // hidden ctor
    ~PWMCtl();  // hidden dtor

    ledc::ch channels[LEDC_SPEED_MODE_MAX*LEDC_CHANNEL_MAX];
    ledc::timer timers[LEDC_SPEED_MODE_MAX*LEDC_TIMER_MAX];

    // construct channel default cfg
    void chInit();
    // construct timers default cfg
    void tmInit();

    // configure channel with current options
    int chCfg(uint32_t ch);

    /**
    * "Fade ended" callback function will be called on any channed fade operation has ended
    * it is called from an ISR, so I just send an event to the common Event Group and let it
    * go outside an ISR
    */
// 
    static bool IRAM_ATTR isr_fade(const ledc_cb_param_t *param, void *arg);
};


