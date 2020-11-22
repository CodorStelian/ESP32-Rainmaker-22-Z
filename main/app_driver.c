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

#define ADDR_BH1750 BH1750_ADDR_LO
#define ADDR_SHT31 SHT3X_I2C_ADDR_GND

#include <iot_button.h>
#include <led_strip.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>

#include <app_reset.h>
#include "app_priv.h"

#define RMT_TX_CHANNEL RMT_CHANNEL_0
/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static uint8_t g_i2c_sda = DEFAULT_I2C_SDA_GPIO;
static uint8_t g_i2c_scl = DEFAULT_I2C_SCL_GPIO;

static uint8_t g_gpio_relay_0 = DEFAULT_OUTPUT_GPIO_RELAY_0;
static uint8_t g_gpio_relay_1 = DEFAULT_OUTPUT_GPIO_RELAY_1;
static uint8_t g_gpio_relay_2 = DEFAULT_OUTPUT_GPIO_RELAY_2;
static uint8_t g_gpio_relay_3 = DEFAULT_OUTPUT_GPIO_RELAY_3;
static bool g_light0_power_state = DEFAULT_LIGHT0_POWER_STATE;
static bool g_light3_power_state = DEFAULT_LIGHT3_POWER_STATE;
static uint16_t g_light0_value = DEFAULT_LIGHT0_BRIGHTNESS;

static led_strip_t *g_rgbpixel_strip;
static esp_timer_handle_t rgbpixel_anim_timer;
static esp_timer_handle_t rgbpixel_anim_duration_timer;
static uint8_t g_gpio_rgbpixel_strip = DEFAULT_OUTPUT_GPIO_RGBPIXEL_STRIP;
static uint8_t g_rgbpixel_strip_pixels = DEFAULT_RGBPIXEL_STRIP_PIXELS;
static bool g_rgbpixel_power_state = DEFAULT_RGBPIXEL_POWER_STATE;
static uint16_t g_rgbpixel_hue = DEFAULT_RGBPIXEL_HUE;
static uint16_t g_rgbpixel_saturation = DEFAULT_RGBPIXEL_SATURATION;
static uint16_t g_rgbpixel_value = DEFAULT_RGBPIXEL_BRIGHTNESS;
uint32_t rgbpixel_spin_blue_bg;
uint32_t rgbpixel_spin_blue_fg;
uint32_t rgbpixel_pulse_blue_min;
uint32_t rgbpixel_pulse_blue_max;
uint32_t rgbpixel_pulse_red_min;
uint32_t rgbpixel_pulse_red_max;
uint32_t rgbpixel_pulse_green_min;
uint32_t rgbpixel_pulse_green_max;
uint8_t rgbpixel_anim_style = 0;
uint8_t rgbpixel_anim_counter = 0;
bool rgbpixel_anim_up = true;

static esp_timer_handle_t bh1750_sensor_timer;
static esp_timer_handle_t sht31_sensor_timer;
static uint16_t g_sensor_luminosity;
static float g_sensor_temperature;
static float g_sensor_humidity;

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

static esp_err_t app_driver_rgbpixel_set_pixel(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    g_rgbpixel_hue = hue;
    g_rgbpixel_saturation = saturation;
    g_rgbpixel_value = brightness;
    led_strip_hsv2rgb(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value, &red, &green, &blue);
	for (int i=0; i<g_rgbpixel_strip_pixels; i++) {
		g_rgbpixel_strip->set_pixel(g_rgbpixel_strip, i, red, green, blue);
	}
    g_rgbpixel_strip->refresh(g_rgbpixel_strip, 100);
    return ESP_OK;
}

uint32_t enhanced_rgbpixel_color(uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

esp_err_t enhanced_rgbpixel_set_pixel(uint16_t n, uint32_t c)
{
	uint8_t r = (uint8_t)(c >> 16);
	uint8_t g = (uint8_t)(c >> 8);
	uint8_t b = (uint8_t)c;
	r = r * g_rgbpixel_value * 0.01f;
	g = g * g_rgbpixel_value * 0.01f;
	b = b * g_rgbpixel_value * 0.01f;
	g_rgbpixel_strip->set_pixel(g_rgbpixel_strip, n, r, g, b);
    return ESP_OK;
}

esp_err_t enhanced_rgbpixel_anim_fill(uint32_t c)
{
	for (int i=0; i<g_rgbpixel_strip_pixels; i++) {
		enhanced_rgbpixel_set_pixel(i, c);
	}
	return ESP_OK;
}

esp_err_t enhanced_rgbpixel_anim_spinner(uint32_t cbg, uint32_t cfg, uint8_t pos)
{
	enhanced_rgbpixel_anim_fill(cbg);
	for (int i=pos; i<pos+2; i++) {
		enhanced_rgbpixel_set_pixel(i % g_rgbpixel_strip_pixels, cfg);
	}
    return ESP_OK;
}

uint32_t enhanced_rgbpixel_interpolate(uint32_t cmin, uint32_t cmax, double t)
{
	uint8_t r = (cmin >> 16 & 0xFF)*(1-t) + (cmax >> 16 & 0xFF)*t;
	uint8_t g = (cmin >> 8  & 0xFF)*(1-t) + (cmax >> 8  & 0xFF)*t;
	uint8_t b = (cmin       & 0xFF)*(1-t) + (cmax       & 0xFF)*t;
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

esp_err_t enhanced_rgbpixel_anim_pulse(uint32_t cmin, uint32_t cmax, double ratio, bool up)
{
	double t = (up) ? ratio : 1 - ratio;
	uint32_t c = enhanced_rgbpixel_interpolate(cmin, cmax, t);
	enhanced_rgbpixel_anim_fill(c);
    return ESP_OK;
}

static void enhanced_rgbpixel_anim(void *priv)
{
		if(rgbpixel_anim_counter <= 23)
		rgbpixel_anim_counter = rgbpixel_anim_counter + 1;
		else
		rgbpixel_anim_counter = 0;
		if (rgbpixel_anim_counter == 0) {
			rgbpixel_anim_up = !rgbpixel_anim_up; // swap
		}

		// 0.0->1.0 per duration
		double ratio = rgbpixel_anim_counter * 0.041;
		if(rgbpixel_anim_style == 0){
			enhanced_rgbpixel_anim_spinner(rgbpixel_spin_blue_bg, rgbpixel_spin_blue_fg, rgbpixel_anim_counter);
		} else if(rgbpixel_anim_style == 1){
			enhanced_rgbpixel_anim_pulse(rgbpixel_pulse_blue_min, rgbpixel_pulse_blue_max, ratio, rgbpixel_anim_up);
		} else if(rgbpixel_anim_style == 2){
			enhanced_rgbpixel_anim_pulse(rgbpixel_pulse_red_min, rgbpixel_pulse_red_max, ratio, rgbpixel_anim_up);
		} else if(rgbpixel_anim_style == 3){
			enhanced_rgbpixel_anim_pulse(rgbpixel_pulse_green_min, rgbpixel_pulse_green_max, ratio, rgbpixel_anim_up);
		}
		g_rgbpixel_strip->refresh(g_rgbpixel_strip, 100);
}

static void enhanced_rgbpixel_anim_duration(void *priv)
{
	esp_timer_stop(rgbpixel_anim_timer);
	ESP_LOGI(TAG, "Enhanced rgbpixel animation is ending now");
	if(!g_rgbpixel_power_state)
		g_rgbpixel_strip->clear(g_rgbpixel_strip, 100);
	else
		app_driver_rgbpixel_set(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value);
}

esp_err_t enhanced_rgbpixel_set_anim(const char *type)
{
	if(strcmp(type, "ERROR") == 0) {
		rgbpixel_anim_style = 2;
	} else if (strcmp(type, "OTA") == 0) {
		rgbpixel_anim_style = 2;
	} else if (strcmp(type, "LOAD") == 0) {
		rgbpixel_anim_style = 0;
	} else if (strcmp(type, "MOVE") == 0) {
		rgbpixel_anim_style = 1;
	}
	esp_timer_start_periodic(rgbpixel_anim_timer, DEFAULT_REFRESH_ANIM_PERIOD_RGBPIXEL * 1000U);
	esp_timer_start_once(rgbpixel_anim_duration_timer, DEFAULT_ANIM_DURATION_RGBPIXEL * 1000000U);
	return ESP_OK;
}

esp_err_t app_driver_rgbpixel_set(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    /* Whenever this function is called, light power will be ON */
    if (!g_rgbpixel_power_state) {
        g_rgbpixel_power_state = true;
		esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(rgb_ring_light, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_rgbpixel_power_state));
    }
    return app_driver_rgbpixel_set_pixel(hue, saturation, brightness);
}

esp_err_t app_driver_rgbpixel_set_power(bool power)
{
    g_rgbpixel_power_state = power;
    if (power) {
        app_driver_rgbpixel_set(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value);
    } else {
        g_rgbpixel_strip->clear(g_rgbpixel_strip, 100);
    }
    return ESP_OK;
}

esp_err_t app_driver_rgbpixel_set_brightness(uint16_t brightness)
{
    g_rgbpixel_value = brightness;
    return app_driver_rgbpixel_set(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value);
}
esp_err_t app_driver_rgbpixel_set_hue(uint16_t hue)
{
    g_rgbpixel_hue = hue;
	return app_driver_rgbpixel_set(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value);
}
esp_err_t app_driver_rgbpixel_set_saturation(uint16_t saturation)
{
    g_rgbpixel_saturation = saturation;
    return app_driver_rgbpixel_set(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value);
}

static void app_driver_sensor_bh1750_update(void *pvParameters)
{
	i2c_dev_t dev17;
    memset(&dev17, 0, sizeof(i2c_dev_t)); // Zero descriptor

    ESP_ERROR_CHECK(bh1750_init_desc(&dev17, ADDR_BH1750, 0, g_i2c_sda, g_i2c_scl));
    ESP_ERROR_CHECK(bh1750_setup(&dev17, BH1750_MODE_CONTINIOUS, BH1750_RES_HIGH));
	
	uint16_t lux;
	if (bh1750_read(&dev17, &lux) != ESP_OK)
		ESP_LOGE(TAG, "BH1750 error, could not read sensor data");
	g_sensor_luminosity = lux;
	esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(luminosity_sensor, "luminosity"),
                esp_rmaker_float(g_sensor_luminosity));
}
static void app_driver_sensor_sht31_update(void *pvParameters)
{
	sht3x_t dev31;
	memset(&dev31, 0, sizeof(sht3x_t)); // Zero descriptor

    ESP_ERROR_CHECK(sht3x_init_desc(&dev31, 0, ADDR_SHT31, g_i2c_sda, g_i2c_scl));
    ESP_ERROR_CHECK(sht3x_init(&dev31));
	
	float temp;
    float humid;
	ESP_ERROR_CHECK(sht3x_measure(&dev31, &temp, &humid));
	g_sensor_temperature = temp;
	g_sensor_humidity = humid;
	esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(temperature_sensor, ESP_RMAKER_PARAM_TEMPERATURE),
                esp_rmaker_float(g_sensor_temperature));
	esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(humidity_sensor, "humidity"),
                esp_rmaker_float(g_sensor_humidity));
}

uint16_t app_driver_sensor_get_current_luminosity()
{
    return g_sensor_luminosity;
}

float app_driver_sensor_get_current_temperature()
{
    return g_sensor_temperature;
}

float app_driver_sensor_get_current_humidity()
{
    return g_sensor_humidity;
}

esp_err_t app_driver_rgbpixel_init(void)
{
	rgbpixel_spin_blue_bg = enhanced_rgbpixel_color(0, 0, 255);
	rgbpixel_spin_blue_fg = enhanced_rgbpixel_color(0, 255, 255);
	rgbpixel_pulse_blue_min = enhanced_rgbpixel_color(0, 0, 255);
	rgbpixel_pulse_blue_max = enhanced_rgbpixel_color(0, 255, 255);
	rgbpixel_pulse_red_min = enhanced_rgbpixel_color(40, 17, 0);
	rgbpixel_pulse_red_max = enhanced_rgbpixel_color(255, 17, 0);
	rgbpixel_pulse_green_min = enhanced_rgbpixel_color(0, 17, 0);
	rgbpixel_pulse_green_max = enhanced_rgbpixel_color(0, 255, 0);
	
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(g_gpio_rgbpixel_strip, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(g_rgbpixel_strip_pixels, (led_strip_dev_t)config.channel);
    g_rgbpixel_strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!g_rgbpixel_strip) {
        ESP_LOGE(TAG, "Install WS2812 driver failed");
        return ESP_FAIL;
    }
    if (g_rgbpixel_power_state) {
        app_driver_rgbpixel_set_pixel(g_rgbpixel_hue, g_rgbpixel_saturation, g_rgbpixel_value);
    } else {
        g_rgbpixel_strip->clear(g_rgbpixel_strip, 100);
    }
	
	esp_timer_create_args_t rgbpixel_anim_timer_conf = {
        .callback = enhanced_rgbpixel_anim,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rgbpixel_anim_tm"
    };
	esp_timer_create_args_t rgbpixel_anim_duration_timer_conf = {
        .callback = enhanced_rgbpixel_anim_duration,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rgbpixel_anim_duration_tm"
    };
	if (esp_timer_create(&rgbpixel_anim_timer_conf, &rgbpixel_anim_timer) != ESP_OK) {
        return ESP_FAIL;
    }
	if (esp_timer_create(&rgbpixel_anim_duration_timer_conf, &rgbpixel_anim_duration_timer) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_driver_sensor_init(void)
{	
	ESP_ERROR_CHECK(i2cdev_init()); // Init Library
    esp_timer_create_args_t bh1750_sensor_timer_conf = {
        .callback = app_driver_sensor_bh1750_update,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "app_driver_sensor_bh1750_update_tm"
    };esp_timer_create_args_t sht31_sensor_timer_conf = {
        .callback = app_driver_sensor_sht31_update,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "app_driver_sensor_sht31_update_tm"
    };
    if (esp_timer_create(&bh1750_sensor_timer_conf, &bh1750_sensor_timer) == ESP_OK) {
        esp_timer_start_periodic(bh1750_sensor_timer, DEFAULT_REPORTING_PERIOD_BH1750 * 1000000U);
    }
	else{
		return ESP_FAIL;
	}
	if (esp_timer_create(&sht31_sensor_timer_conf, &sht31_sensor_timer) == ESP_OK) {
        esp_timer_start_periodic(sht31_sensor_timer, DEFAULT_REPORTING_PERIOD_SHT31 * 1000000U);
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
	app_driver_set_light0_power(new_light0_state);
	esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(bedroom_light, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(new_light0_state));
}

static void set_light0_power_state(bool target)
{
	gpio_set_level(g_gpio_relay_0, target);
}
static void set_light1_power_state(bool target)
{
	gpio_set_level(g_gpio_relay_1, target);
}
static void set_light2_power_state(bool target)
{
	gpio_set_level(g_gpio_relay_2, target);
}
static void set_light3_power_state(bool target)
{
	gpio_set_level(g_gpio_relay_3, target);
}

void app_driver_init()
{
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
		/* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    /* Configure the GPIO */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
	uint64_t pin_mask = (((uint64_t)1 << g_gpio_relay_0) | ((uint64_t)1 << g_gpio_relay_1) | ((uint64_t)1 << g_gpio_relay_2) | ((uint64_t)1 << g_gpio_relay_3));
    io_conf.pin_bit_mask = pin_mask;
    gpio_config(&io_conf);
	app_driver_sensor_init();
	app_driver_rgbpixel_init();
}

int IRAM_ATTR app_driver_set_light0()
{
	if(g_light0_power_state){
		if(g_light0_value == 0){
			set_light0_power_state(0);
			set_light1_power_state(0);
			set_light2_power_state(0);
		}
		else if(g_light0_value >= 1 && g_light0_value <=25) {
			set_light0_power_state(1);
			set_light1_power_state(0);
			set_light2_power_state(0);
		}
		else if(g_light0_value >= 26 && g_light0_value <=50) {
			set_light0_power_state(1);
			set_light1_power_state(0);
			set_light2_power_state(1);
		}
		else if(g_light0_value >= 51 && g_light0_value <=75) {
			set_light0_power_state(1);
			set_light1_power_state(1);
			set_light2_power_state(0);
		}
		else if(g_light0_value >= 76) {
			set_light0_power_state(1);
			set_light1_power_state(1);
			set_light2_power_state(1);
		}
	}
	else {
		set_light0_power_state(0);
		set_light1_power_state(0);
		set_light2_power_state(0);
	}
	return ESP_OK;
}

esp_err_t app_driver_set_light0_power(bool power)
{
    g_light0_power_state = power;
    app_driver_set_light0();
    return ESP_OK;
}
esp_err_t app_driver_set_light0_brightness(uint16_t brightness)
{
    g_light0_value = brightness;
	g_light0_power_state = 1;
	esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(bedroom_light, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_light0_power_state));
    return app_driver_set_light0();
}

int IRAM_ATTR app_driver_set_light3_state(bool state)
{
	if(g_light3_power_state != state) {
		g_light3_power_state = state;
		set_light3_power_state(g_light3_power_state);
	}
	return ESP_OK;
}

bool app_driver_get_light0_state(void)
{
	return g_light0_power_state;
}

bool app_driver_get_light3_state(void)
{
	return g_light3_power_state;
}