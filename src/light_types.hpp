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
#include "luma_curves.hpp"


enum class lightsource_t:uint8_t {
    generic,            // any unspecific light source
    constant,           // i.e. ordinary lamps
    dimmable,           // dimmable sources
    rgb,                // any rgb's, including rgbw, rgbww, etc...
    dynamic,            // all kinds of addressable leds, etc...
    composite           // light units with more than one light source
};

enum class power_share_t:uint8_t {
    incremental,        // a set of lights combined
    equal,              // all sources are constantly equal
    phaseshift          // sources try to do max load in a round robbin time slots (phase-shifted PWM)
};


struct light_state_t {
    lightsource_t ltype;
    luma::curve luma;
    int32_t fadetime;           // default fade time duration
    int32_t brtscale;           // default scale for brightness
    int32_t increment;          // default increment step
    uint32_t value;
    uint32_t value_max;
    uint32_t value_scaled;
    float power;
    float power_max;
    bool active_ll;             // active logic level
};

