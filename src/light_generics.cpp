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

// BIT macro's
#define BIT_SET(var, bit) (var |= 1<<bit)
#define BIT_CLR(var, bit) (var &= ~(1<<bit))
#define BIT_READ(var, bit) ((var >> bit) & 1)

static const char* TAG = "light_gnrc";

void GenericLight::goValue(uint32_t value, int32_t duration){

    if(luma != luma::curve::linear)
        value = curveMap(luma, value, getMaxValue(), getMaxValue());        // map to luma curve if non-linear

    if (duration < 0)
        duration = fadetime;

    ESP_LOGD(TAG, "goValue val:%d, duration:%d", value, duration);
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

light_state_t GenericLight::getState() const{
    light_state_t state = {
        ltype,      //    lightsource_t ltype;
        luma,       //    luma::curve luma;
        fadetime,   //    int32_t fadetime;           // default fade time duration
        brtscale,   //    int32_t brtscale;           // default scale for brightness
        increment,  //    int32_t increment;          // default increment step
        getValue(), //    uint32_t value;
        getMaxValue(),          //    uint32_t value_max;
        getValueScaled(),       //    uint32_t value_scaled;
        getCurrentPower(),      //    float power;
        power,                  //    float power_max;
        getActiveLogicLevel()   //    bool active_ll;             // active logic level
    };
    return state;
}



// ********************************************************
// CompositeLight methods
CompositeLight::CompositeLight(GenericLight *gl, uint8_t id, power_share_t share) : GenericLight(lightsource_t::composite, 0, gl->getCurve()), sub_type(gl->getLType()), ps(share) {
    combined_value = 0;
    addLight(gl, id);
}

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
        default : {     // power_share_t::incremental
            combined_value += node->light->getMaxValue();           // default is the sum of max values of all the lights in stack

            if (node->light->getCurve() == luma::curve::binary){    // if constant lights are stacked, than curve mapping mutates to linear
                luma = luma::curve::linear;
            }
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
    // curve cant't be changed for constant lights
    if (sub_type == lightsource_t::constant)
        return luma;

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
            ESP_LOGD(TAG, "Composite incremental: set val:%d/%d", m, value);
            value -= m;
            continue;
        }

        _i->get()->light->fade_to_value(value, duration);   // выставляем остаток
        value = 0;                                          // остальные источники гасим
        ESP_LOGD(TAG, "Composite incremental: set val:%d", value);
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
    uint32_t flags = 0;
    uint32_t channel = 0;
    for (auto _i = ls.begin(); _i != ls.end(); ++_i){
        DimmableLight *l = static_cast<DimmableLight*>(_i->get()->light.get());
        uint32_t duty_shift = value * channel % l->getMaxValue();
        ESP_LOGD(TAG, "Phase-shifted PWM: ls:%d, duty:%d, dty-shift:%d\n", channel, value, duty_shift);

        if (duration){

            if ( l->getDutyShift() + value > l->getMaxValue() ){   // new duty value can't be reached, need phase down-shifting first
                l->setDutyShift(duty_shift);
                l->fade_to_value(value, duration);
            } else {    // if ( l->getValue() + duty_shift > l->getMaxValue() ) // new duty_shift can't be set with current duty
                l->fade_to_value(value, duration);
                BIT_SET(flags, channel);
                // l->setDutyShift(value, duty_shift);              // for LEDC driver chennel is blocked until fade is over
                                                                    // need to WA this in some ugly manner, so I postpone DutyShift operation
                                                                    // till ALL the channels equeued fade operation
                                                                    // otherwise fading would be blocked. Fading for channles will be executed one by one
                                                                    // this is an ugly hack for now. A better approach would be to use queue mecanism on light driver's level
            }
        } else
            l->setDutyShift(value, duty_shift);

        ++channel;
    }

    // another iteration to apply new duty_shift value after fade task has been equeued to all channels
    // TODO: use some queueing at driver's level
    if (flags){
        channel = 0;
        for (auto _i = ls.begin(); _i != ls.end(); ++_i){
            if (BIT_READ(flags, channel)) {     // shift only affected channels here
                DimmableLight *l = static_cast<DimmableLight*>(_i->get()->light.get());
                uint32_t duty_shift = value * channel % l->getMaxValue();
                l->setDutyShift(value, duty_shift);
            }

            ++channel;
        }
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



