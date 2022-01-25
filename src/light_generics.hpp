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
#include <memory>
#include "LList.h"

#define DEFAULT_FADE_TIME           1000           // ms

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
    interleave          // sources try to do max load in a round robbin time slots (phased PWM) 
};


class GenericLight {

protected:
    lightsource_t const ltype;
    luma::curve luma;
    float power;

    virtual void set_to_value(uint32_t value) = 0;            // pure virtual
    inline virtual void fade_to_value(uint32_t value, uint32_t duration){ return set_to_value(value); };
    friend class LightUnit;

public:
    GenericLight(lightsource_t type = lightsource_t::generic, float pwr = 1.0, luma::curve lcurve = luma::curve::linear) : ltype(type), power(pwr), luma(lcurve){};
    virtual ~GenericLight(){};

    // Brightness functions
    virtual void goValue(uint32_t value, uint32_t duration = DEFAULT_FADE_TIME);
    inline virtual void goMax(uint32_t duration = DEFAULT_FADE_TIME){ return goValue( getMaxValue(), duration); };
    inline virtual void goMin(uint32_t duration = DEFAULT_FADE_TIME){ return goValue(1, duration); };
    inline virtual void goOn(uint32_t duration = DEFAULT_FADE_TIME){  return goMax(duration); };
    inline virtual void goOff(uint32_t duration = DEFAULT_FADE_TIME){ return goValue(0, duration); };
    inline virtual void goStep(int32_t step, uint32_t duration = DEFAULT_FADE_TIME){ return goValue( getValue() + step, duration); };

    inline virtual void pwr(bool state, uint32_t duration = DEFAULT_FADE_TIME){ state ? goOn(duration) : goOff(duration); };
    inline virtual void fade(bool state, uint32_t duration){ return pwr(state, duration); };

    virtual void goValueScaled(uint32_t value, uint32_t scale=100, uint32_t duration = DEFAULT_FADE_TIME);
    virtual void goStepScaled(int32_t step, uint32_t scale=100, uint32_t duration = DEFAULT_FADE_TIME);

    // set methods
    inline virtual luma::curve setCurve( luma::curve curve) { luma = curve; return luma; };
    virtual float setMaxPower(float p);

    // get methods
    virtual lightsource_t getLType() const { return ltype; }

    virtual uint32_t getValue() const = 0;                      // pure virtual
    virtual uint32_t getMaxValue() const = 0;                   // pure virtual
    inline virtual uint32_t getValueScaled(uint32_t scale=100) const { return luma::curveUnMap(luma, getValue(), getMaxValue(), scale);};

    inline virtual luma::curve getCurve() const { return luma; };

    virtual float getMaxPower() const { return power; }
    virtual float getCurrentPower() const;

};


class ConstantLight : public GenericLight {
public:
    ConstantLight(float power = 1.0) : GenericLight(lightsource_t::constant, power, luma::curve::binary){};
    luma::curve setCurve( luma::curve curve) { return luma; };
    uint32_t getMaxValue() const override { return 1; }
    virtual float getCurrentPower() const override { return getMaxValue(); };
};


class DimmableLight : public GenericLight {
public:
    DimmableLight(float power = 1.0, luma::curve lcurve = luma::curve::linear) : GenericLight(lightsource_t::dimmable, power, lcurve){};

    // Dimmable light specific methods
    virtual void setPWM(uint8_t resolution, uint32_t freq) = 0;
};




class CompositeLight : public GenericLight {

    struct LightSource {
        uint8_t id;
        std::unique_ptr<GenericLight> light;
    };


    lightsource_t const sub_type;
    power_share_t ps;

    //std::unique_ptr<char[]> descr;                  // Mnemonic name for the instance
    LList<std::shared_ptr<LightSource>> ls;        // list of registered Ligh Sources objects

    /**
     * @brief check if light source with specified id is already registered
     * 
     * @param id 
     * @return true 
     * @return false 
     */
    bool exist(uint8_t id) const;

public:
    CompositeLight(lightsource_t type, power_share_t share = power_share_t::incremental) : sub_type(type), ps(share), GenericLight(lightsource_t::composite, 0){};
    CompositeLight(GenericLight *gl, uint8_t id = 1, power_share_t share = power_share_t::incremental);

    bool addLight(GenericLight *gl, uint8_t id);

};