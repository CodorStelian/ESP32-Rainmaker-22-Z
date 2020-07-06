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

#define DEFAULT_I2C_SDA_GPIO 21
#define DEFAULT_I2C_SCL_GPIO 22

#define DEFAULT_OUTPUT_GPIO_RGBPIXEL_STRIP 5
#define DEFAULT_OUTPUT_GPIO_RELAY_0 19
#define DEFAULT_OUTPUT_GPIO_RELAY_1 18
#define DEFAULT_OUTPUT_GPIO_RELAY_2 17
#define DEFAULT_OUTPUT_GPIO_RELAY_3 16

#define DEFAULT_LIGHT0_POWER_STATE false
#define DEFAULT_LIGHT1_POWER_STATE false

#define DEFAULT_RGBPIXEL_STRIP_PIXELS 24
#define DEFAULT_RGBPIXEL_POWER_STATE false
#define DEFAULT_RGBPIXEL_HUE         180
#define DEFAULT_RGBPIXEL_SATURATION  100
#define DEFAULT_RGBPIXEL_BRIGHTNESS  15
#define DEFAULT_REFRESH_ANIM_PERIOD_RGBPIXEL 40 /* Miliseconds */
#define DEFAULT_CHANGE_ANIM_PERIOD_RGBPIXEL 10 /* Seconds */

#define DEFAULT_REPORTING_PERIOD_BH1750    60 /* Seconds */
#define DEFAULT_REPORTING_PERIOD_SHT31    305 /* Seconds */

void app_driver_init(void);

int app_driver_set_light0_state(bool state);
int app_driver_set_light1_state(bool state);
bool app_driver_get_light0_state(void);
bool app_driver_get_light1_state(void);

esp_err_t app_driver_rgbpixel_set(uint32_t hue, uint32_t saturation, uint32_t brightness);
esp_err_t app_driver_rgbpixel_set_power(bool power);
esp_err_t app_driver_rgbpixel_set_brightness(uint16_t brightness);
esp_err_t app_driver_rgbpixel_set_hue(uint16_t hue);
esp_err_t app_driver_rgbpixel_set_saturation(uint16_t saturation);

uint16_t app_driver_sensor_get_current_luminosity();
float app_driver_sensor_get_current_temperature();
float app_driver_sensor_get_current_humidity();
