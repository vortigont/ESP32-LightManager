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

#include "light_drv_ledc.hpp"

void LEDCLight::fade_to_value(uint32_t value, int32_t duration){
    if (fc && duration)
        fc->fadebyTime(ch, value, duration);
    else
        set_to_value(value);
};

void LEDCLight::setPWM(uint8_t resolution, uint32_t freq){
    if (resolution >= LEDC_TIMER_BIT_MAX)
        resolution = LEDC_TIMER_BIT_MAX - 1;

    pwm->tmSet(pwm->chGetTimernum(ch), (ledc_timer_bit_t)resolution, freq);
}

void LEDCLight::setDutyShift(uint32_t duty, uint32_t dshift){
    if (dshift > getMaxValue())
        dshift = getMaxValue();

    pwm->chDutyPhase(ch, duty, dshift);
}
