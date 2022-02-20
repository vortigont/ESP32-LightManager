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

#include "esp32ledc_fader.hpp"

// LOGGING
#ifdef ARDUINO
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

static const char* TAG = "ledc_fader";

#define EVT_TASK_NAME   "FADE_EVT"
#define EVT_TASK_STACK  2048
#define EVT_TASK_PRIO   2

using namespace luma;


// *** *** //
// FadeEngineHW methods
bool FadeEngineHW::fade(uint32_t duty, uint32_t duration){
    return !ledc_set_fade_time_and_start(
      PWMCtl::getInstance()->chGet(channel)->cfg.speed_mode,
      PWMCtl::getInstance()->chGet(channel)->cfg.channel,
      duty,
      duration,
      LEDC_FADE_NO_WAIT);
}



/*
bool PWMCtl::fader_enable(bool state){
  if (state){     // install LEDC ISR
    if (ledc_fade_func_install(0))
      faderIRQ = false;
    else
      faderIRQ = true;
  } else {
    ledc_fade_func_uninstall();
    faderIRQ = false;
  }
  return faderIRQ;
}
*/

FadeCtrl::FadeCtrl( uint32_t mask ) : events_mask(mask){
  pwm = PWMCtl::getInstance();
  eg_fade_evt = pwm->getFaderEventGroup();

  //Create a task to handle fade events from ISR
  if (eg_fade_evt)
    xTaskCreate(FadeCtrl::evtTask, EVT_TASK_NAME, EVT_TASK_STACK, (void *)this, EVT_TASK_PRIO, &t_fade_evt);

}

FadeCtrl::~FadeCtrl(){
  vTaskDelete(t_fade_evt);
}

void FadeCtrl::fd_events_handler(){
  ESP_LOGI(TAG, "Start fade events listener task");

  EventBits_t x;

  for (;;){
    x = xEventGroupWaitBits(
      *eg_fade_evt,
      events_mask,    // group bits for monitored channels
      pdTRUE,         // clear bits on return
      pdFALSE,        // any bit set triggers event
      (portTickType)portMAX_DELAY
    );

    //x &= events_mask;
    int i = 0;
    do {
      if (x & 1){
        ESP_LOGD(TAG, "fade end event ch:%d\n", i);
        if (chf[i].cb)
          chf[i].cb(i, fade_event_t::fade_end);
        //printf(" %d", i);
      }
      x >>= 1;
    } while (++i < LEDC_CHANNEL_MAX);
    //printf("\n");
  }
  vTaskDelete(NULL);
}

bool FadeCtrl::nofade(uint8_t ch, uint32_t duty){
    ESP_LOGD(TAG, "nofade ch:%d, duty:%d\n", ch, duty);
    return PWMCtl::getInstance()->chDuty(ch, duty);
};

bool FadeCtrl::setFader(uint8_t ch, fade_engine_t fe, fe_callback_t f){
    ch %= LEDC_SPEED_MODE_MAX*LEDC_CHANNEL_MAX;

    ESP_LOGD(TAG, "setFader ch:%d, err:%d\n", ch, PWMCtl::getInstance()->chFadeISR(ch, true));
    //printf("setFader ch:%d, err:%d\n", ch, PWMCtl::getInstance()->chFadeISR(ch, true));

    if (f)
      chf[ch].cb = std::move(f);

    // todo: пересоздавать объект при повторном вызове или пропускать?
    if (!chf[ch].fe){
      chf[ch].fe = new FadeEngineHW(ch);
      return chf[ch].fe ? true : false;
    }

    return false;
}

bool FadeCtrl::fadebyTime(uint8_t ch, uint32_t duty, uint32_t duration){
    ch %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX;
    if (!chf[ch].fe){
      return nofade(ch, duty);  // do a no-fade duty change if no FadeEngine installed for the channel 
    }

    if(chf[ch].fe->fade(duty, duration)){
      if (chf[ch].cb)
        chf[ch].cb(ch, fade_event_t::fade_start);
      return true;
    } else
      return false;
}

