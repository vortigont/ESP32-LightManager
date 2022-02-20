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
#include <functional>
#include "LList.h"

#define DEFAULT_FADE_TIME           1000            // ms
#define DEFAULT_SCALE               100             // fits percent control 0%-100%
#define DEFAULT_SCALE_STEP          10              // 10% step
#define USE_DEFAULT                 -1              // Use light object's own setting


typedef std::function<void ()> callback_t;
//typedef std::function<void (event_t event, const event_args*)> callback_t;

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


class GenericLight {
friend class CompositeLight;

protected:
    lightsource_t const ltype;
    float power;
    luma::curve luma;
    int32_t fadetime = DEFAULT_FADE_TIME;           // default fade time duration
    int32_t brtscale = DEFAULT_SCALE;               // default scale for brightness
    int32_t increment = DEFAULT_SCALE / 10;         // default increment step

    callback_t callback = nullptr;                  // external callback function to call on state change

    /**
     * @brief run external callback function
     * every time objects state changes a callback triggered to notify
     */
    virtual void onChange(){ if (callback) callback(); };

    /**
     * @brief set normalized power/brightness level
     * 
     * @param value - normalized value
     */
    virtual void set_to_value(uint32_t value) = 0;            // pure virtual

    /**
     * @brief fade power/brightness to normalized value
     * if fade is not implemented than a direct set_to_value() called
     * @param value - normalized value
     * @param duration - fade duration in ms
     */
    virtual void fade_to_value(uint32_t value, int32_t duration){ return set_to_value(value); };    // should be overriden with drivers supporting fade

public:
    GenericLight(lightsource_t type = lightsource_t::generic, float pwr = 1.0, luma::curve lcurve = luma::curve::linear) : ltype(type), power(pwr), luma(lcurve){};
    virtual ~GenericLight(){};

    // Brightness functions
    virtual void goValue(uint32_t value, int32_t duration = USE_DEFAULT);

    inline virtual void goMax(int32_t duration = USE_DEFAULT){ return goValue( getMaxValue(), duration); };
    inline virtual void goMin(int32_t duration = USE_DEFAULT){ return goValue(1, duration); };
    inline virtual void goOn(int32_t duration = USE_DEFAULT){  return goMax(duration); };
    inline virtual void goOff(int32_t duration = USE_DEFAULT){ return goValue(0, duration); };
    virtual void goToggle(int32_t duration = USE_DEFAULT);
    virtual void   goIncr(int32_t duration = USE_DEFAULT){ return goStepScaled(increment, brtscale, duration); };
    virtual void   goDecr(int32_t duration = USE_DEFAULT){ return goStepScaled(-1*increment, brtscale, duration); };

    virtual void goStep(int32_t step, int32_t duration = USE_DEFAULT){ return goValue( getValue() + step, duration); };

    virtual void  goStepScaled(int32_t step, int32_t scale=USE_DEFAULT, int32_t duration = USE_DEFAULT);

    virtual void goValueScaled(uint32_t value, int32_t scale=USE_DEFAULT, int32_t duration = USE_DEFAULT);

    inline virtual void pwr(bool state, int32_t duration = USE_DEFAULT){ state ? goOn(duration) : goOff(duration); };


    // set methods
    inline virtual luma::curve setCurve( luma::curve curve) { luma = curve; return luma; };
    virtual float setMaxPower(float p);

    /**
     * @brief Set active logic level to HIGH or LOW
     * i.e. could be required to inverse on/off logic or PWM active level
     * default is HIGH
     * 
     * @param invert 
     * 
     * @return true 
     * @return false 
     */
    virtual bool setActiveLogicLevel(bool lvl){ return true; };

    /**
     * @brief Get active logic level to HIGH or LOW
     * 
     * @return true if light is inverted
     * @return false otherwise
     */
    virtual bool getActiveLogicLevel() const { return true; };

    // get methods
    virtual lightsource_t getLType() const { return ltype; }

    virtual uint32_t getValue() const = 0;                      // pure virtual
    virtual uint32_t getMaxValue() const = 0;                   // pure virtual
    virtual uint32_t getValueScaled(int32_t scale=USE_DEFAULT) const;

    inline virtual luma::curve getCurve() const { return luma; };

    virtual float getMaxPower() const { return power; }
    virtual float getCurrentPower() const;


    /**
     * @brief attach external callback function
     * every time objects state changes a callback triggered to notify
     * 
     * @param f callback function prototype: std::function<void ()>
     */
    void onChangeAttach(callback_t f){ if (f) callback = std::move(f); };

    /**
     * @brief detach external callback function
     * 
     */
    void onChangeDetach(){ callback = nullptr; };
};


class ConstantLight : public GenericLight {
public:
    ConstantLight(float power = 1.0) : GenericLight(lightsource_t::constant, power, luma::curve::binary){};
    luma::curve setCurve( luma::curve curve) { return luma; };
    uint32_t getMaxValue() const override { return 1; }
    float getCurrentPower() const override { return getMaxPower(); };
};


class DimmableLight : public GenericLight {
public:
    DimmableLight(float power = 1.0, luma::curve lcurve = luma::curve::linear) : GenericLight(lightsource_t::dimmable, power, lcurve){};

    // PWM Dimmable light specific methods
    virtual void setPWM(uint8_t resolution, uint32_t freq) = 0;

    virtual void setPhaseShift(int degrees){};

    /**
     * @brief Set Duty Shift value for the light source if supported by backend driver
     * 
     * @param dshift duty value 0 - MAX_DUTY_VALUE 
     */
    virtual void setDutyShift(uint32_t duty, uint32_t dshift){};

    virtual int getPhaseShift(){ return 0; };

    virtual uint32_t getDutyShift(){ return 0; };
};




class CompositeLight : public GenericLight {

    struct LightSource {
        uint8_t id;
        std::unique_ptr<GenericLight> light;
    };


    lightsource_t const sub_type;
    power_share_t ps;
    uint32_t combined_value = 0;                    // проверить на возможное /0

    //std::unique_ptr<char[]> descr;                  // Mnemonic name for the instance
    LList<std::shared_ptr<LightSource>> ls;         // list of registered Ligh Sources objects

    /**
     * @brief check if light source with specified id is already registered
     * 
     * @param id 
     * @return true 
     * @return false 
     */
    bool exist(uint8_t id) const;

    /**
     * @brief Get current brightness Value for incremental power-share mode
     * 
     * @return uint32_t brightness Value
     */
    uint32_t getValue_incremental() const;

    /**
     * @brief Set the to value for incremental-type lights
     * it sets brt value incrementaly starting from the first added soruce
     * 
     * @param value 
     */
    void goValueIncremental(uint32_t value, int32_t duration);

    /**
     * @brief Set the to value for equal-type lights
     * i.e. non-dimming lights, or synchronous PWMs
     * all lights get's the same settings as first light added the container
     * 
     * @param value - brightness value in range 0-MAX_BRIGHTNESS of a first light
     * @param duration - fade duration (if supported by backend driver)
     */
    void goValueEqual(uint32_t value, int32_t duration);

    /**
     * @brief a selector for specific methods depending on power_share type
     * 
     * @param value
     * @param duration
     */
    void goValueComposite(uint32_t value, int32_t duration);

    /**
     * @brief Set the to value for dimmable lights
     * operates like goValueEqual() with an addition for PWM drivers
     * supporting per channel phase control. Resulting waveforms would be
     * shifted in time to achive as much uniform load as possible.
     * The goal is make PWM dimming more even-spreaded
     * - provide less flicker or even flickerless PWM
     * - reduce power supply stressing with 0 to 100% transitions like with sync PWM
     * - reduce audible noise from PSU and FET drivers feeding LEDs
     * 
     * @param value - brightness value in range 0-MAX_BRIGHTNESS of a first light
     * @param duration - fade duration (if supported by backend driver)
     */
    void goValuePhaseShift(uint32_t value, int32_t duration);

    // *** overrides *** //
    inline void set_to_value(uint32_t value) override { goValueComposite(value, 0); };
    inline void fade_to_value(uint32_t value, int32_t duration) override { goValueComposite(value, duration); };

public:
    CompositeLight(lightsource_t type, power_share_t share = power_share_t::incremental) : GenericLight(lightsource_t::composite, 0), sub_type(type), ps(share){};
    CompositeLight(GenericLight *gl, uint8_t id = 1, power_share_t share = power_share_t::incremental);

    // *** Parent Methods Overrides ***
    // set methods
    luma::curve setCurve( luma::curve curve) override;

    float setMaxPower(float p) override { return power; }     // combined power change is not supported

    // get methods
    uint32_t getValue() const override;
    inline uint32_t getMaxValue() const override { return combined_value; }
    float getCurrentPower() const override;

    // Own methods

    /**
     * @brief add a light source to the container
     * combined power will be a sum of all light sources,
     * depending on power_share type combined_value (max brightness value)
     * might be a sum of max_values for each sources or fixed value to the first light sources added
     * 
     * @param gl a pointer to GenricLight object or it's derivateve actually (pointer will become invalidated on adding)
     * @param id any random id for the added light source
     * @return true on success addition
     * @return false otherwise
     */
    bool addLight(GenericLight *gl, uint8_t id);


    /**
     * @brief Get GenericLight object via id
     * 
     * @param id requested obj id
     * @return GenericLight* if such id exist
     * @return nullptr otherwise
     */
    GenericLight *getLight(uint8_t id);

};