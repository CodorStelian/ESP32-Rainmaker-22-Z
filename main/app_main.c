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
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>

#include <app_wifi.h>

#include "app_priv.h"

static const char *TAG = "app_main";

extern const char ota_server_cert[] asm("_binary_server_crt_start");

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t rmaker_app_callback(const char *dev_name, const char *name, esp_rmaker_param_val_t val, void *priv_data)
{
	if(strcmp(dev_name, "RGB Light") == 0) {
		if (strcmp(name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
			ESP_LOGI(TAG, "Received value = %s for %s - %s",
					val.val.b? "true" : "false", dev_name, name);
			app_driver_rgbpixel_set_power(val.val.b);
		} else if (strcmp(name, "Brightness") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, dev_name, name);
			app_driver_rgbpixel_set_brightness(val.val.i);
		} else if (strcmp(name, "Hue") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, dev_name, name);
			app_driver_rgbpixel_set_hue(val.val.i);
		} else if (strcmp(name, "Saturation") == 0) {
			ESP_LOGI(TAG, "Received value = %d for %s - %s",
					val.val.i, dev_name, name);
			app_driver_rgbpixel_set_saturation(val.val.i);
		}
	}
	else if(strcmp(dev_name, "Bedroom Light") == 0) {
		ESP_LOGI(TAG, "Received value = %s for %s - %s",
				val.val.b? "true" : "false", dev_name, name);
		if (strcmp(name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
			app_driver_set_light0_state(val.val.b);
		}
	}
	else if(strcmp(dev_name, "Wall Light") == 0) {
		ESP_LOGI(TAG, "Received value = %s for %s - %s",
				val.val.b? "true" : "false", dev_name, name);
		if (strcmp(name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
			app_driver_set_light1_state(val.val.b);
		}
	}
	else{
		/* Silently ignoring invalid params */
		return ESP_OK;
	}
	esp_rmaker_update_param(dev_name, name, val);
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
        .info = {
            .name = "Smart Home 22-Z",
            .type = "Device Hub",
        },
        .enable_time_sync = false,
    };
    err = esp_rmaker_init(&rainmaker_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialise ESP RainMaker. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a Light device and add the relevant parameters to it */
	esp_rmaker_create_lightbulb_device("Bedroom Light", rmaker_app_callback, NULL, DEFAULT_LIGHT0_POWER_STATE);
	esp_rmaker_create_lightbulb_device("Wall Light", rmaker_app_callback, NULL, DEFAULT_LIGHT1_POWER_STATE);
	
	esp_rmaker_create_lightbulb_device("RGB Light", rmaker_app_callback, NULL, DEFAULT_RGBPIXEL_POWER_STATE);
	esp_rmaker_device_add_brightness_param("RGB Light", "Brightness", DEFAULT_RGBPIXEL_BRIGHTNESS);
	esp_rmaker_device_add_hue_param("RGB Light", "Hue", DEFAULT_RGBPIXEL_HUE);
	esp_rmaker_device_add_saturation_param("RGB Light", "Saturation", DEFAULT_RGBPIXEL_SATURATION);

	esp_rmaker_create_temp_sensor_device("Temperature Sensor", NULL, NULL, app_driver_sensor_get_current_temperature());
	
	esp_rmaker_create_device("Humidity Sensor", NULL, NULL, NULL);
	esp_rmaker_device_add_name_param("Humidity Sensor", ESP_RMAKER_DEF_NAME_PARAM);
	esp_rmaker_device_add_param("Humidity Sensor", "humidity", esp_rmaker_float(app_driver_sensor_get_current_humidity()), PROP_FLAG_READ);
	esp_rmaker_device_assign_primary_param("Humidity Sensor", "humidity");
	
	esp_rmaker_create_device("Luminosity Sensor", NULL, NULL, NULL);
	esp_rmaker_device_add_name_param("Luminosity Sensor", ESP_RMAKER_DEF_NAME_PARAM);
	esp_rmaker_device_add_param("Luminosity Sensor", "luminosity", esp_rmaker_float(app_driver_sensor_get_current_luminosity()), PROP_FLAG_READ);
	esp_rmaker_device_assign_primary_param("Luminosity Sensor", "luminosity");
	
	/* Enable OTA */
	esp_rmaker_ota_config_t ota_config = {
		.server_cert = ota_server_cert,
	};
	esp_rmaker_ota_enable(&ota_config, OTA_USING_PARAMS);

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    app_wifi_start();
}