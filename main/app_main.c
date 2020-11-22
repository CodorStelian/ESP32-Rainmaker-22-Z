/* Multi-Device Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>

#include <app_wifi.h>

#include "app_priv.h"

static const char *TAG = "app_main";

esp_rmaker_device_t *bedroom_light;
esp_rmaker_device_t *wall_light;
esp_rmaker_device_t *rgb_ring_light;
esp_rmaker_device_t *temperature_sensor;
esp_rmaker_device_t *humidity_sensor;
esp_rmaker_device_t *luminosity_sensor;

extern const char ota_server_cert[] asm("_binary_server_crt_start");

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
	const char *device_name = esp_rmaker_device_get_name(device);
    const char *param_name = esp_rmaker_param_get_name(param);
	if(strcmp(device_name, "RGB Light") == 0) {
		if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
			ESP_LOGI(TAG, "Received value = %s for %s - %s",
					val.val.b? "true" : "false", device_name, param_name);
			app_driver_rgbpixel_set_power(val.val.b);
		} else if (strcmp(param_name, "Brightness") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, device_name, param_name);
			app_driver_rgbpixel_set_brightness(val.val.i);
		} else if (strcmp(param_name, "Hue") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, device_name, param_name);
			app_driver_rgbpixel_set_hue(val.val.i);
		} else if (strcmp(param_name, "Saturation") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, device_name, param_name);
			app_driver_rgbpixel_set_saturation(val.val.i);
		}
	}
	else if(strcmp(device_name, "Bedroom Light") == 0) {
		if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
			ESP_LOGI(TAG, "Received value = %s for %s - %s",
					val.val.b? "true" : "false", device_name, param_name);
			app_driver_set_light0_power(val.val.b);
		} else if (strcmp(param_name, "Brightness") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, device_name, param_name);
			app_driver_set_light0_brightness(val.val.i);
		}
	}
	else if(strcmp(device_name, "Wall Light") == 0) {
		ESP_LOGI(TAG, "Received value = %s for %s - %s",
				val.val.b? "true" : "false", device_name, param_name);
		if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
			app_driver_set_light3_state(val.val.b);
		}
	}
	else{
		/* Silently ignoring invalid params */
		return ESP_OK;
	}
	enhanced_rgbpixel_set_anim("LOAD");
	esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    app_driver_init();

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_init()
     */
    app_wifi_init();
    
    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
   esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "Smart Home 22-Z", "Device Hub");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a Light device and add the relevant parameters to it */
	bedroom_light = esp_rmaker_lightbulb_device_create("Bedroom Light", NULL, DEFAULT_LIGHT0_POWER_STATE);
    esp_rmaker_device_add_cb(bedroom_light, write_cb, NULL);
	esp_rmaker_device_add_param(bedroom_light, esp_rmaker_brightness_param_create("Brightness", DEFAULT_LIGHT0_BRIGHTNESS));
	esp_rmaker_node_add_device(node, bedroom_light);
	
   /* Create a Light device and add the relevant parameters to it */
	wall_light = esp_rmaker_lightbulb_device_create("Wall Light", NULL, DEFAULT_LIGHT3_POWER_STATE);
    esp_rmaker_device_add_cb(wall_light, write_cb, NULL);
	esp_rmaker_node_add_device(node, wall_light);
	
   /* Create a Light device and add the relevant parameters to it */
	rgb_ring_light = esp_rmaker_lightbulb_device_create("RGB Light", NULL, DEFAULT_RGBPIXEL_POWER_STATE);
    esp_rmaker_device_add_cb(rgb_ring_light, write_cb, NULL);
	esp_rmaker_device_add_param(rgb_ring_light, esp_rmaker_brightness_param_create("Brightness", DEFAULT_RGBPIXEL_BRIGHTNESS));
    esp_rmaker_device_add_param(rgb_ring_light, esp_rmaker_hue_param_create("Hue", DEFAULT_RGBPIXEL_HUE));
    esp_rmaker_device_add_param(rgb_ring_light, esp_rmaker_saturation_param_create("Saturation", DEFAULT_RGBPIXEL_SATURATION));
	esp_rmaker_node_add_device(node, rgb_ring_light);

	/* Create a Temperature Sensor device and add the relevant parameters to it */
    temperature_sensor = esp_rmaker_temp_sensor_device_create("Temperature Sensor", NULL, app_driver_sensor_get_current_temperature());
    esp_rmaker_node_add_device(node, temperature_sensor);

	/* Create a Humidity Sensor device and add the relevant parameters to it */
	humidity_sensor = esp_rmaker_device_create("Humidity Sensor", NULL, NULL);
	esp_rmaker_device_add_param(humidity_sensor, esp_rmaker_name_param_create("name", "Humidity Sensor"));
	esp_rmaker_param_t *humidity_param = esp_rmaker_param_create("humidity", NULL, esp_rmaker_float(app_driver_sensor_get_current_humidity()), PROP_FLAG_READ);
	esp_rmaker_device_add_param(humidity_sensor, humidity_param);
	esp_rmaker_device_assign_primary_param(humidity_sensor, humidity_param);
	esp_rmaker_node_add_device(node, humidity_sensor);
	
	/* Create a Luminosity Sensor device and add the relevant parameters to it */
	luminosity_sensor = esp_rmaker_device_create("Luminosity Sensor", NULL, NULL);
	esp_rmaker_device_add_param(luminosity_sensor, esp_rmaker_name_param_create("name", "Luminosity Sensor"));
	esp_rmaker_param_t *luminosity_param = esp_rmaker_param_create("luminosity", NULL, esp_rmaker_float(app_driver_sensor_get_current_humidity()), PROP_FLAG_READ);
	esp_rmaker_device_add_param(luminosity_sensor, luminosity_param);
	esp_rmaker_device_assign_primary_param(luminosity_sensor, luminosity_param);
	esp_rmaker_node_add_device(node, luminosity_sensor);

	/* Enable OTA */
	esp_rmaker_ota_config_t ota_config = {
		.server_cert = ota_server_cert,
	};
	esp_rmaker_ota_enable(&ota_config, OTA_USING_PARAMS);

	/* Enable scheduling */
    esp_rmaker_schedule_enable();
	
    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

   /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}