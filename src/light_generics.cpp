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

#include "light_generics.hpp"
#include <string.h>

// LOGGING
#ifdef ARDUINO
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

static const char* TAG = "light_gnrc";

void GenericLight::goValue(uint32_t value, int32_t duration){

    if(luma != luma::curve::linear)
        value = curveMap(luma, value, getMaxValue(), getMaxValue());        // map to luma curve if non-linear

    if (duration < 0)
        duration = fadetime;

    ESP_LOGD(TAG, "goValue val:%d, duration:%d\n", value, duration);
    fade_to_value(value, duration);
};

void GenericLight::goValueScaled(uint32_t value, int32_t scale, int32_t duration){
    if (scale <= 0)
        scale = brtscale;

    if (value >= scale)
        return goMax(duration);

    if (value == 0)
        return goOff(duration);

    ESP_LOGD(TAG, "val:%d, scale:%d, duration:%d\n", value, scale, duration);

    return fade_to_value( curveMap(luma, value, getMaxValue(), scale), duration );
}

void GenericLight::goStepScaled(int32_t step, int32_t scale, int32_t duration){
    if (!step)
        return;

    uint32_t cur = getValueScaled(scale);
    if (cur + step <= 0)     // do not go to negative
        return goOff();

    ESP_LOGD(TAG, "step:%d, scale: %d, current value:%d, new value:%d duration:%d\n", step, scale, cur, step + cur, duration);
    return goValueScaled(step + cur, scale, duration);
}

float GenericLight::setMaxPower(float p){
    if (p < 0)
        power = 0;
    else
        power = p;
    
    return p;
}

float GenericLight::getCurrentPower() const {
    return getMaxPower() * getValue() / getMaxValue();
}

void GenericLight::goToggle(int32_t duration){
    if (getValue())
        goOff(duration);
    else
        goOn(duration);
}

uint32_t GenericLight::getValueScaled(int32_t scale) const {
    if (scale <=0)
        scale = brtscale;
    return luma::curveUnMap(luma, getValue(), getMaxValue(), scale);
}




// ********************************************************
// CompositeLight methods
CompositeLight::CompositeLight(GenericLight *gl, uint8_t id, power_share_t share) : GenericLight(lightsource_t::composite, 0, gl->getCurve()), sub_type(gl->getLType()), ps(share) {
    combined_value = 0;
    addLight(gl, id);
};

bool CompositeLight::exist(uint8_t id) const {
    for (auto _i = ls.cbegin(); _i != ls.cend(); ++_i){
        if (_i->get()->id == id)
            return true;
    }
    return false;
}

bool CompositeLight::addLight(GenericLight *gl, uint8_t id){
    if (exist(id))
        return false;       // light with such id already exist

    if (gl->getLType() != sub_type)
        return false;       // added light source does not match, can't mix different light sources (yet)

    auto node = std::make_shared<LightSource>();
    node->id = id;
    node->light.reset(std::move(gl));

    if (!ls.add(node))
        return false;

    switch(ps){
        case power_share_t::equal :
        case power_share_t::phaseshift : {
            if (ls.size() == 1)
                combined_value = node->light->getMaxValue();        // MaxValue will be the same for all sources defined by first added node
            break;
        }
        default : {                                                 // default is sum up max values of all lights
            combined_value += node->light->getMaxValue();
        }
    }

    power += node->light->getMaxPower();
    return true;
}


GenericLight* CompositeLight::getLight(uint8_t id){
    for (auto _i = ls.cbegin(); _i != ls.cend(); ++_i){
        if (_i->get()->id == id)
            return _i->get()->light.get();
    }
    return nullptr;
}


uint32_t CompositeLight::getValue_incremental() const {
    uint32_t val = 0;
    for (auto _i = ls.cbegin(); _i != ls.cend(); ++_i){
        val += _i->get()->light->getValue();
    }
    return val;
}


uint32_t CompositeLight::getValue() const {
    switch(ps){
        case power_share_t::equal :
        case power_share_t::phaseshift :{
            if (ls.size()){
                return ls.head()->light->getValue();                    // all lights are equal, get the first one and return it's value
            }
            return 0;
        }
        default :
            return getValue_incremental();
    }
}

float CompositeLight::getCurrentPower() const {
    switch(ps){
        case power_share_t::equal : {
            if (ls.size())
                return ls.head()->light->getCurrentPower() * ls.size();     // all lights are equal, get the first one and multiply by the number of lights
            return 0;
        }
        default : {
            float val = 0.0;
            for (auto _i = ls.cbegin(); _i != ls.cend(); ++_i){
                val += _i->get()->light->getCurrentPower();
            }
            return val;
        }
    }
}

luma::curve CompositeLight::setCurve( luma::curve curve){
    luma = curve;
    for (auto _i = ls.begin(); _i != ls.end(); ++_i){
        _i->get()->light->setCurve(curve);
    }
    return luma;
}


void CompositeLight::goValueIncremental(uint32_t value, int32_t duration){
    for (auto _i = ls.begin(); _i != ls.end(); ++_i){
        uint32_t m = _i->get()->light->getMaxValue();

        if (value >= m){    // кратное увеличение яркости
            _i->get()->light->fade_to_value(m, duration);
            value -= m;
            continue;
        }

        _i->get()->light->fade_to_value(value, duration);   // выставляем остаток
        value = 0;                                          // остальные источники гасим
    }
}

void CompositeLight::goValueEqual(uint32_t value, int32_t duration){
    for (auto _i = ls.begin(); _i != ls.end(); ++_i){
        _i->get()->light->fade_to_value(value, duration);
    }
}

void CompositeLight::goValueComposite(uint32_t value, int32_t duration){
    if (!ls.size())
        return;         // skip if container is empty

    switch(ps){
        case power_share_t::equal :
            return goValueEqual(value, duration);
        case power_share_t::phaseshift :
            return goValuePhaseShift(value, duration);
        default :
            return goValueIncremental(value, duration);
    }
}


void CompositeLight::goValuePhaseShift(uint32_t value, int32_t duration){
    // check if we hold dimmable lights, otherwise use 'equal' control
    if (ls.head()->light->getLType() != lightsource_t::dimmable)
        return goValueEqual(value, duration);

    // calculate per-source duty offset for phase-shifted PWM
    int channel = 0;
    for (auto _i = ls.begin(); _i != ls.end(); ++_i){
        uint32_t duty_shift = value * channel % _i->get()->light->getMaxValue();
          printf("Phase shifted PWM: ls:%d, duty:%d, phase:%d\n", channel, value, duty_shift);
        ++channel;
        DimmableLight *l = static_cast<DimmableLight*>(_i->get()->light.get());

        l->setDutyShift(value, duty_shift);
        if (duration)
            l->fade_to_value(value, duration);
    }
}


/*
LightUnit::LightUnit(const uint16_t _id, lightsource_t type, const char *_descr) : id(_id), ltype(type){
    if (!_descr || !*_descr){
        descr.reset(new char[9]);   // i.e. LU-42
        sprintf(descr.get(), "LU-%d", id);
    } else
        descr.reset(strcpy(new char[strlen(_descr) + 1], _descr));
};
*/



