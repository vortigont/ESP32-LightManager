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

#include "esp32ledc.hpp"

#ifdef ARDUINO
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

// ESP32 log tag
static const char *TAG __attribute__((unused)) = "LEDC";

// static member must be defined outside the class scope
EventGroupHandle_t PWMCtl::g_fade_evt = nullptr;


PWMCtl::PWMCtl(){
  tmInit();
  chInit();
  // fade_func required for thread safe ledc_set_duty_and_update() function to work
  ledc_fade_func_install(0);
}

PWMCtl::~PWMCtl(){
  ledc_fade_func_uninstall();
}
    //t_cfg.speed_mode = (ledc_mode_t)(i/LEDC_TIMER_MAX);
    //t_cfg.timer_num = (ledc_timer_t)(i%LEDC_TIMER_MAX);

// construct channel default cfg
void PWMCtl::chInit(){
  ledc_channel_config_t c_cfg = {
    -1,                       // int gpio_num;                   /*!< the LEDC output gpio_num, if you want to use gpio16, gpio_num = 16 */
    LEDC_LOW_SPEED_MODE,      // ledc_mode_t speed_mode;         /*!< LEDC speed speed_mode, high-speed mode or low-speed mode */
    LEDC_CHANNEL_0,           // ledc_channel_t channel;         /*!< LEDC channel (0 - 7) */
    LEDC_INTR_DISABLE,        // ledc_intr_type_t intr_type;     /*!< configure interrupt, Fade interrupt enable  or Fade interrupt disable */
    LEDC_TIMER_0,             // ledc_timer_t timer_sel;         /*!< Select the timer source of channel (0 - 3) */
    DEFAULT_PWM_DUTY,         // uint32_t duty;                  /*!< LEDC channel duty, the range of duty setting is [0, (2**duty_resolution)-1] */
    0,                        // int hpoint;                     /*!< LEDC channel hpoint value, the max value is 0xfffff */
    {0}                       // unsigned int output_invert: 1;/*!< Enable (1) or disable (0) gpio output invert */
  };

  for (unsigned i = 0; i < LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX; ++i){
    channels[i].cfg = c_cfg;
    channels[i].cfg.speed_mode = (ledc_mode_t)(i/LEDC_CHANNEL_MAX);
    channels[i].cfg.channel = (ledc_channel_t)(i%LEDC_CHANNEL_MAX);
  }
}

// set channel duty
esp_err_t PWMCtl::chDuty(uint32_t ch, uint32_t duty){
  return chDutyPhase(ch, duty, channels[ch % (LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX)].cfg.hpoint);
}

esp_err_t PWMCtl::chPhase(uint32_t ch, uint32_t phase){
  return chDutyPhase(ch, channels[ch % (LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX)].cfg.duty, phase);
}

esp_err_t PWMCtl::chDutyPhase(uint32_t ch, uint32_t duty, uint32_t phase){
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;
  //phase %= LEDC_HPOINT_VAL_MAX;

  channels[ch].cfg.duty = duty;
  channels[ch].cfg.hpoint = phase;

  ESP_LOGD(TAG, "Set Channel:%d, duty:%d, phase:%d\n", ch, duty, phase);

  /* this method does not change hpoint value
    Looks like it's been fixed here - https://github.com/espressif/esp-idf/commit/e175086226405ca5dfd0b0cdde917b0ad8330827
    and merged in https://github.com/espressif/esp-idf/commit/3821a09f8369d510cf9d1669967bfc25f68dd783
    Arduino's 2.0.2 is quite old, so works only for idf build
  */
#ifdef LEDC_DUTY_SETNUPDATE
  return ledc_set_duty_and_update(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, duty, phase);
#endif

  // Use this as a hackish workaround for older Arduino frameworks
  if (phase)
    ledc_set_duty_with_hpoint(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, duty, phase);
  else
    ledc_set_duty(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, duty);
  return ledc_update_duty(channels[ch].cfg.speed_mode, channels[ch].cfg.channel);
};

uint32_t PWMCtl::chGetDuty(uint32_t ch) const {
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;
  return ledc_get_duty(channels[ch].cfg.speed_mode, channels[ch].cfg.channel);
}

int PWMCtl::chStart(uint32_t ch, int pin){
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;

  if (pin > 0)
    channels[ch].cfg.gpio_num = pin;

  // check if we are already running
  if (channels[ch].state == ch_state::active)
    return ESP_OK;

  // configure channel
  if (chCfg(ch))
    return ESP_ERR_INVALID_STATE;

  if (chFadeISR(ch, channels[ch].fade_cb))
    return ESP_ERR_INVALID_STATE;

  // run attached Timer (if needed)
  if (tmStart(chGetTimernum(ch)))
    return ESP_ERR_INVALID_STATE;

  channels[ch].state = ch_state::active;
  printf("channel:%d started as LEDC ch:%d, mode:%d\n", ch, channels[ch].cfg.channel, channels[ch].cfg.speed_mode);
  return ESP_OK;
}

int PWMCtl::chStop(uint32_t ch){
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;
  return ledc_stop(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, channels[ch].idle_level);
}

int PWMCtl::chAttachTimer(uint32_t ch, uint8_t timer){
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;
  timer %= LEDC_TIMER_MAX;

  channels[ch].cfg.timer_sel = (ledc_timer_t)(timer);
  return ledc_bind_channel_timer(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, channels[ch].cfg.timer_sel);
}

int PWMCtl::chSet(uint32_t ch, int pin, bool idlelvl, bool invert){
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;

  printf("Configuring pin %d for ch:%d / ledcch:%d\n", pin, ch, channels[ch].cfg.channel);

  if (channels[ch].cfg.channel >= LEDC_CHANNEL_MAX){
    printf("Cfg corrupted for ch:%d / ledcch:%d\n", ch, channels[ch].cfg.channel);
    return ESP_ERR_INVALID_STATE;
  }

  channels[ch].cfg.gpio_num = pin;
  channels[ch].cfg.flags.output_invert = invert;
  channels[ch].idle_level = idlelvl;

  ledc_stop(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, channels[ch].idle_level);
  return chCfg(ch);
}

int PWMCtl::chCfg(uint32_t ch){
  if (channels[ch].cfg.gpio_num == -1){
    printf("err pin is not set for ch:%d\n", ch);
    return ESP_ERR_INVALID_STATE;
  }  // pin is not set

  if (ledc_channel_config(&channels[ch].cfg)){
    printf("err cfg ch:%d\n", ch);
    channels[ch].state = ch_state::stop;
    return ESP_ERR_INVALID_STATE;
  }

  printf("cfg set for ch:%d\n", ch);
  return ESP_OK;
}

int PWMCtl::chFadeISR(uint32_t ch, bool enable){
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;
  channels[ch].fade_cb = enable;

  if (channels[ch].fade_cb){
    ledc_cbs_t cbs = { .fade_cb = isr_fade };
    return ledc_cb_register(channels[ch].cfg.speed_mode, channels[ch].cfg.channel, &cbs, nullptr); // (void *)ch
  }
  return ESP_OK;
  // TODO: is there any way to detach channel fade ISR cb? /it, is!/
}

uint8_t PWMCtl::chGetTimernum(int32_t ch) const {
  ch %= LEDC_SPEED_MODE_MAX * LEDC_CHANNEL_MAX;
  return channels[ch].cfg.timer_sel + ch/LEDC_CHANNEL_MAX * LEDC_TIMER_MAX;
}


// timers
void PWMCtl::tmInit(){
  ledc_timer_config_t t_cfg = {
    LEDC_LOW_SPEED_MODE,      // .speed_mode
    DEFAULT_PWM_RESOLUTION,   // .duty_resolution / resolution of PWM duty
    LEDC_TIMER_0,             // .timer_num / timer index
    DEFAULT_PWM_FREQ,         // .freq_hz / frequency of PWM signal
    DEFAULT_PWM_CLK           // .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
  };

  for (unsigned i = 0; i < LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX; ++i){
    timers[i].cfg = t_cfg;
    timers[i].cfg.speed_mode = (ledc_mode_t)(i/LEDC_TIMER_MAX);
    timers[i].cfg.timer_num = (ledc_timer_t)(i%LEDC_TIMER_MAX);
  }
}

int PWMCtl::tmStart(uint8_t tm){
    tm %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX;
    if ((uint8_t)timers[tm].state > 0)
      return ESP_OK;

    if(ledc_timer_config(&timers[tm].cfg)){
      printf("err cfg timer:%d\n", tm);
      timers[tm].state = tm_state::stop;
      return ESP_ERR_INVALID_STATE;
    }

    printf("Configured timer %d\n", tm);
    timers[tm].state = tm_state::active;
    return ESP_OK;
}

esp_err_t PWMCtl::tmSet(uint8_t tm, ledc_timer_bit_t bits, uint32_t hz){
  tm %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX;
  timers[tm].cfg.duty_resolution = bits;
  timers[tm].cfg.freq_hz = hz;
  return ledc_timer_config(&timers[tm].cfg);
}

esp_err_t PWMCtl::tmSetFreq(uint8_t tm, uint32_t hz){
  tm %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX;
  timers[tm].cfg.freq_hz = hz;
  return ledc_set_freq(timers[tm].cfg.speed_mode, timers[tm].cfg.timer_num, hz);
}

uint32_t PWMCtl::tmGetFreq(uint8_t tm) const {
  tm %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX;
  return ledc_get_freq(timers[tm].cfg.speed_mode, timers[tm].cfg.timer_num);
}

EventGroupHandle_t* PWMCtl::getFaderEventGroup(){
  // create MsgGroup
  if (!g_fade_evt)
    g_fade_evt = xEventGroupCreate();

  return &g_fade_evt;
}

bool IRAM_ATTR PWMCtl::isr_fade(const ledc_cb_param_t *param, void *arg){
        portBASE_TYPE taskAwoken = pdFALSE;

        if (g_fade_evt && (param->event == LEDC_FADE_END_EVT)) {
            uint32_t ch = param->channel * (param->speed_mode ? 2 : 1);
            xEventGroupSetBitsFromISR(g_fade_evt, (1<<ch), &taskAwoken);
            //xEventGroupSetBitsFromISR(g_fade_evt, (1<<(uint32_t)arg), &taskAwoken);
        }

        return (taskAwoken == pdTRUE);  // a context switch may take place in a calee isr
    }


uint32_t PWMCtl::chGetMaxDuty(uint32_t ch) const {
    ch %= LEDC_SPEED_MODE_MAX * LEDC_TIMER_MAX;   // return wrap_ledc_get_max_duty(channels[ch].cfg.speed_mode, channels[ch].cfg.channel); 

    uint8_t chtimer = channels[ch].cfg.timer_sel;
    if (ch / LEDC_CHANNEL_MAX)      // check if it's a LS esp32 channel
      chtimer += LEDC_TIMER_MAX;

    return (1 << timers[chtimer].cfg.duty_resolution) - 1;
};
