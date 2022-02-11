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

GPIOLight::GPIOLight(gpio_num_t pin, float power, bool lvl) : all(lvl), GenericLight(lightsource_t::constant, power, luma::curve::binary){
    gpio_init(pin, lvl);
}

GPIOLight::GPIOLight(int pin, float power, bool lvl) : all(lvl), GenericLight(lightsource_t::constant, power, luma::curve::binary){
    gpio_init(static_cast<gpio_num_t>(pin), lvl);
}

void GPIOLight::gpio_init(gpio_num_t pin, bool active_level){
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)){
        //ESP_LOGE(TAG, "pin:%d can't be used as OUTPUT\n", pin);
        gpio = GPIO_NUM_NC;
        return;
    }

    gpio_reset_pin(gpio);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1 << gpio;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    // Invert pin logic level if required
    if (!all)
        GPIO.func_out_sel_cfg[gpio].inv_sel = 1;

    gpio_set_level(gpio, 0);        // initialize as "disabled" or "off"
}

bool GPIOLight::setActiveLogicLevel(bool lvl){
    all = lvl;
    GPIO.func_out_sel_cfg[gpio].inv_sel = !all;
    return all;
}
