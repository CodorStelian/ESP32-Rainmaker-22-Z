/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#define DEFAULT_STRIP_LED_NUMBER 24
#define DEFAULT_RGB_LIGHT_POWER true
#define DEFAULT_LIGHT_POWER false
#define DEFAULT_HUE         180
#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  15
#define REPORTING_PERIOD_BH1750    60 /* Seconds */
#define REPORTING_PERIOD_SHT31    305 /* Seconds */

void app_driver_init(void);
esp_err_t app_light_set(uint32_t hue, uint32_t saturation, uint32_t brightness);
esp_err_t app_light_set_power(bool power);
esp_err_t app_light_set_brightness(uint16_t brightness);
esp_err_t app_light_set_hue(uint16_t hue);
esp_err_t app_light_set_saturation(uint16_t saturation);
esp_err_t app_driver_set_gpio(const char *dev_name, bool value);
uint16_t app_get_current_luminosity();
float app_get_current_temperature();
float app_get_current_humidity();
