/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <sdkconfig.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/rmt.h>
#include <bh1750.h>
#include <sht3x.h>


#define SDA_GPIO 21
#define SCL_GPIO 22

#define ADDR_BH1750 BH1750_ADDR_LO
#define ADDR_SHT31 SHT3X_I2C_ADDR_GND

#include <iot_button.h>
#include <led_strip.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>

#include "app_priv.h"

#define RMT_TX_CHANNEL RMT_CHANNEL_0
/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0
/* This is the GPIO on which the power will be set */
#define OUTPUT_GPIO_RGB_STRIP 5
#define OUTPUT_GPIO_RELAY_0 19
#define OUTPUT_GPIO_RELAY_1 18
#define OUTPUT_GPIO_RELAY_2 17
#define OUTPUT_GPIO_RELAY_3 16

static uint16_t g_luminosity;
static float g_temperature;
static float g_humidity;
static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;
static led_strip_t *g_strip;
static esp_timer_handle_t bh1750_sensor_timer;
static esp_timer_handle_t sht31_sensor_timer;
static bool g_light0_power_state = DEFAULT_LIGHT0_POWER_STATE;
static bool g_light1_power_state = DEFAULT_LIGHT1_POWER_STATE;
static bool g_rgb_power_state = DEFAULT_RGB_LIGHT_POWER_STATE;
static const char *TAG = "app_driver";

static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
	h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

static esp_err_t app_light_set_led(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    g_hue = hue;
    g_saturation = saturation;
    g_value = brightness;
    led_strip_hsv2rgb(g_hue, g_saturation, g_value, &red, &green, &blue);
	for (int i=0; i<DEFAULT_STRIP_LED_NUMBER; i++) {
		g_strip->set_pixel(g_strip, i, red, green, blue);
	}
    g_strip->refresh(g_strip, 100);
    return ESP_OK;
}
esp_err_t app_light_set(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    /* Whenever this function is called, light power will be ON */
    if (!g_rgb_power_state) {
        g_rgb_power_state = true;
        esp_rmaker_update_param("RGB Light", ESP_RMAKER_DEF_POWER_NAME, esp_rmaker_bool(g_rgb_power_state));
    }
    return app_light_set_led(hue, saturation, brightness);
}

esp_err_t app_light_set_power(bool power)
{
    g_rgb_power_state = power;
    if (power) {
        app_light_set(g_hue, g_saturation, g_value);
    } else {
        g_strip->clear(g_strip, 100);
    }
    return ESP_OK;
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_value = brightness;
    return app_light_set(g_hue, g_saturation, g_value);
}
esp_err_t app_light_set_hue(uint16_t hue)
{
    g_hue = hue;
	return app_light_set(g_hue, g_saturation, g_value);
}
esp_err_t app_light_set_saturation(uint16_t saturation)
{
    g_saturation = saturation;
    return app_light_set(g_hue, g_saturation, g_value);
}

static void app_sensor_bh1750_update(void *pvParameters)
{
	i2c_dev_t dev17;
    memset(&dev17, 0, sizeof(i2c_dev_t)); // Zero descriptor

    ESP_ERROR_CHECK(bh1750_init_desc(&dev17, ADDR_BH1750, 0, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(bh1750_setup(&dev17, BH1750_MODE_CONTINIOUS, BH1750_RES_HIGH));
	uint16_t lux;

        if (bh1750_read(&dev17, &lux) != ESP_OK)
			ESP_LOGE(TAG, "BH1750 error, could not read sensor data");
		g_luminosity = lux;
    esp_rmaker_update_param("Luminosity Sensor", ESP_RMAKER_DEF_TEMPERATURE_NAME, esp_rmaker_float(g_luminosity)); 
}
static void app_sensor_sht31_update(void *pvParameters)
{
	sht3x_t dev31;
	memset(&dev31, 0, sizeof(sht3x_t)); // Zero descriptor

    ESP_ERROR_CHECK(sht3x_init_desc(&dev31, 0, ADDR_SHT31, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(sht3x_init(&dev31));
	float temp;
    float humid;
	ESP_ERROR_CHECK(sht3x_measure(&dev31, &temp, &humid));
	g_temperature = temp;
	g_humidity = humid;
	esp_rmaker_update_param("Temperature Sensor", ESP_RMAKER_DEF_TEMPERATURE_NAME, esp_rmaker_float(g_temperature)); 
	esp_rmaker_update_param("Humidity Sensor", ESP_RMAKER_DEF_TEMPERATURE_NAME, esp_rmaker_float(g_humidity));
}

uint16_t app_get_current_luminosity()
{
    return g_luminosity;
}

float app_get_current_temperature()
{
    return g_temperature;
}

float app_get_current_humidity()
{
    return g_humidity;
}

esp_err_t app_light_init(void)
{
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(OUTPUT_GPIO_RGB_STRIP, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(DEFAULT_STRIP_LED_NUMBER, (led_strip_dev_t)config.channel);
    g_strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!g_strip) {
        ESP_LOGE(TAG, "Install WS2812 driver failed");
        return ESP_FAIL;
    }
    if (g_rgb_power_state) {
        app_light_set_led(g_hue, g_saturation, g_value);
    } else {
        g_strip->clear(g_strip, 100);
    }
    return ESP_OK;
}

esp_err_t app_sensor_init(void)
{	
	ESP_ERROR_CHECK(i2cdev_init()); // Init Library
    esp_timer_create_args_t bh1750_sensor_timer_conf = {
        .callback = app_sensor_bh1750_update,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "app_sensor_bh1750_update_tm"
    };esp_timer_create_args_t sht31_sensor_timer_conf = {
        .callback = app_sensor_sht31_update,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "app_sensor_sht31_update_tm"
    };
    if (esp_timer_create(&bh1750_sensor_timer_conf, &bh1750_sensor_timer) == ESP_OK) {
        esp_timer_start_periodic(bh1750_sensor_timer, REPORTING_PERIOD_BH1750 * 1000000U);
    }
	else{
		return ESP_FAIL;
	}
	if (esp_timer_create(&sht31_sensor_timer_conf, &sht31_sensor_timer) == ESP_OK) {
        esp_timer_start_periodic(sht31_sensor_timer, REPORTING_PERIOD_SHT31 * 1000000U);
    }
	else{
		return ESP_FAIL;
	}
	return ESP_OK;
}

static void push_btn_cb(void *arg)
{
	ESP_LOGI(TAG, "Change state of Bedroom Light and sync it with cloud");
	bool new_light0_state = !g_light0_power_state;
	app_driver_set_light0_state(new_light0_state);
	esp_rmaker_update_param("Bedroom Light", ESP_RMAKER_DEF_POWER_NAME, esp_rmaker_bool(new_light0_state));
}

static void button_press_3sec_cb(void *arg)
{
	ESP_LOGW(TAG, "Erase flash in progress, restart is required");
    nvs_flash_deinit();
    nvs_flash_erase();
    esp_restart();
}

static void set_light0_power_state(bool target)
{
	gpio_set_level(OUTPUT_GPIO_RELAY_0, target);
}

static void set_light1_power_state(bool target)
{
	gpio_set_level(OUTPUT_GPIO_RELAY_1, target);
}

void app_driver_init()
{
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
		iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, push_btn_cb, "RELEASE");
        iot_button_add_on_press_cb(btn_handle, 3, button_press_3sec_cb, NULL);
    }

    /* Configure power */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
	uint64_t pin_mask = (((uint64_t)1 << OUTPUT_GPIO_RELAY_0) | ((uint64_t)1 << OUTPUT_GPIO_RELAY_1) | ((uint64_t)1 << OUTPUT_GPIO_RELAY_2) | ((uint64_t)1 << OUTPUT_GPIO_RELAY_3));
    io_conf.pin_bit_mask = pin_mask;
    /* Configure the GPIO */
    gpio_config(&io_conf);
	app_sensor_init();
	app_light_init();
}

int IRAM_ATTR app_driver_set_light0_state(bool state)
{
	if(g_light0_power_state != state) {
		g_light0_power_state = state;
		set_light0_power_state(g_light0_power_state);
	}
	return ESP_OK;
}

int IRAM_ATTR app_driver_set_light1_state(bool state)
{
	if(g_light1_power_state != state) {
		g_light1_power_state = state;
		set_light1_power_state(g_light1_power_state);
	}
	return ESP_OK;
}

bool app_driver_get_light0_state(void)
{
	return g_light0_power_state;
}

bool app_driver_get_light1_state(void)
{
	return g_light1_power_state;
}