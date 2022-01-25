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

void GenericLight::goValue(uint32_t value, uint32_t duration){

    if(luma != luma::curve::linear)
        value = curveMap(luma, value, getMaxValue(), getMaxValue());        // map to luma curve if non-linear

    if (!duration)
        set_to_value(value);

    printf("goValue val:%d, duration:%d\n", value, duration);
    fade_to_value(value, duration);
};

void GenericLight::goValueScaled(uint32_t value, uint32_t scale, uint32_t duration){
    if (value >= scale)
        return goMax(duration);

    if (value == 0)
        return goOff(duration);

    printf("goValueScaled: val:%d, duration:%d, md: %d\n", value, duration, getMaxValue());
    //printf("CurveMap: c:%d, val:%d, maxd:%d, scale: %d\n", chf[ch].l_curve, value, PWMCtl::getInstance()->chGetMaxDuty(ch), scale);

    if (duration)
        return fade_to_value( curveMap(luma, value, getMaxValue(), scale), duration );
    else
        return set_to_value( curveMap(luma, value, getMaxValue(), scale) );
}

void GenericLight::goStepScaled(int32_t step, uint32_t scale, uint32_t duration){
    if (!step)
        return;

    return goValueScaled(step + curveUnMap(luma, getValue(), getMaxValue(), scale), scale, duration);
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


// CompositeLight methods
CompositeLight::CompositeLight(GenericLight *gl, uint8_t id, power_share_t share) : sub_type(gl->getLType()), ps(share), GenericLight(lightsource_t::composite, gl->getMaxPower(), gl->getCurve()){
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
    auto node = std::make_shared<LightSource>();
    if (exist(id))
        return false;

    node->id = id;
    node->light.reset(std::move(gl));
    if (ls.add(node)){
        power += node->light->getMaxPower();
        return true;
    }
    return false;
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

