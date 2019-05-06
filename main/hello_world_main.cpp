/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <ctime>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <esp_sntp.h>

#include "I2C.h"
#include "BME280.h"
#include "ssd1306.hpp"

extern "C" {
   void app_main();
}

static void obtain_time(void) {
	esp_wifi_start();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    //sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI("ntp", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

    esp_wifi_stop();
}

static EventGroupHandle_t s_wifi_event_group;
volatile bool got_ip = false;
static void event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	ESP_LOGI("wifi", "wifi disconnected");
    	got_ip = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("wifi", "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, BIT(0));
        got_ip = true;
    }
}

void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {};
    wifi_config.sta = {};
    strcpy((char*)wifi_config.sta.ssid, "MARS-GUEST");
    strcpy((char*)wifi_config.sta.password, "internet");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    //ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifi", "wifi_init_sta finished.");
}

void app_main()
{
	nvs_flash_init();
	wifi_init_sta();
	obtain_time();

	gpio_config_t cfg;
	cfg.intr_type = GPIO_INTR_DISABLE;
	cfg.mode = GPIO_MODE_OUTPUT;
	cfg.pin_bit_mask = GPIO_SEL_13;
	cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
	cfg.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio_config(&cfg);

	I2C i2c = I2C();
	i2c.init(GPIO_NUM_5, GPIO_NUM_4);

	char* buf = new char[80];

	OLED oled = OLED(&i2c, SSD1306_128x64);
	oled.init();
	oled.select_font(1);

	BME280* bme = new BME280(i2c);
	bme280_reading_data data;

	time_t t1;
	tm* t2;
	while(1) {
		esp_wifi_start();

		t1 = time(NULL);
		t2 = localtime(&t1);
		printf(asctime(t2));
		oled.draw_string(0, 0, asctime(t2), WHITE, BLACK);

        gpio_set_level(GPIO_NUM_13, 1);
		bme->init();
		data = bme->readSensorData();
        gpio_set_level(GPIO_NUM_13, 0);

		sprintf(buf, "%.1f deg %.0f%% RH %.1f kPa", data.temperature * 9 / 5 + 32, data.humidity, data.pressure / 1000);
		printf("%s\n",  buf);
		oled.draw_string(0, 13, buf, WHITE, BLACK);

    	oled.refresh(true);
		oled.fill_rectangle(0, 0, 128, 64, BLACK);

		while(!got_ip) {
		    vTaskDelay(1000/portTICK_PERIOD_MS);
		}
	    esp_wifi_stop();

	    vTaskDelay(10000/portTICK_PERIOD_MS);
	}
}
