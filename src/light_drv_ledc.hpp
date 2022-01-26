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
#include "light_generics.hpp"
#include "esp32ledc_fader.hpp"

class LEDCLight : public DimmableLight {

protected:
    int gpio;
    uint32_t ch;
    PWMCtl *pwm;
    FadeCtrl *fc;

    virtual void set_to_value(uint32_t value) override { PWMCtl::getInstance()->chDuty(ch, value); };
    virtual void fade_to_value(uint32_t value, uint32_t duration){ fc->fadebyTime(ch, value, duration); };

public:
    LEDCLight(uint32_t channel, int pin, FadeCtrl *fader = nullptr, luma::curve lcurve = luma::curve::cie1931, float power = 1.0) : ch(channel), gpio(pin), fc(fader), DimmableLight(power, lcurve){
        pwm = PWMCtl::getInstance();
        pwm->chStart(ch, gpio);
    };
    virtual ~LEDCLight(){};

    inline virtual uint32_t getValue() const override { return PWMCtl::getInstance()->chGetDuty(ch); };
    inline virtual uint32_t getMaxValue() const override { return PWMCtl::getInstance()->chGetMaxDuty(ch); };

    PWMCtl *pwmGet(){ return pwm; };
    void setPWM(uint8_t resolution, uint32_t freq){};
};