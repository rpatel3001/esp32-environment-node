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
#include <esp_http_client.h>
#include <esp_sleep.h>

#include "I2C.h"
#include "BME280.h"
#include "ssd1306.hpp"

extern "C" {
   void app_main();
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    return ESP_OK;
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

RTC_DATA_ATTR int bootCount = 0;
void app_main() {
	I2C i2c = I2C();
	i2c.init(GPIO_NUM_5, GPIO_NUM_4);

	OLED oled = OLED(&i2c, SSD1306_128x64);
	oled.init();
	oled.select_font(1);
	oled.fill_rectangle(0, 0, 128, 64, BLACK);
	oled.draw_string(0, 0, "Starting...", WHITE, BLACK);
	oled.refresh(true);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {};
    wifi_config.sta = {};
    strcpy((char*)wifi_config.sta.ssid, "Patels");
    strcpy((char*)wifi_config.sta.password, "rajananandriya");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

	esp_wifi_start();

	oled.fill_rectangle(0, 0, 128, 64, BLACK);
	oled.draw_string(0, 0, "Connecting to wifi...", WHITE, BLACK);
	oled.refresh(true);

	while(!got_ip) {
	    vTaskDelay(100/portTICK_PERIOD_MS);
	}

	setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();

    if(bootCount == 0) {
    	oled.fill_rectangle(0, 0, 128, 64, BLACK);
    	oled.draw_string(0, 0, "Getting the time...", WHITE, BLACK);
    	oled.refresh(true);

        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        //sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        sntp_init();

        // wait for time to be set
        time_t now = 0;
        struct tm timeinfo = {};
        int retry = 0;
        const int retry_count = 100;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
            ESP_LOGI("ntp", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        time(&now);
        localtime_r(&now, &timeinfo);
    }
	esp_http_client_handle_t report_client;
	esp_http_client_config_t report_config = {};
    report_config.url = "https://rajanpatel.net/api/esp_report";
    report_config.event_handler = _http_event_handle;
    report_config.method = HTTP_METHOD_POST;

	gpio_config_t gpio_cfg;
	gpio_cfg.intr_type = GPIO_INTR_DISABLE;
	gpio_cfg.mode = GPIO_MODE_OUTPUT;
	gpio_cfg.pin_bit_mask = GPIO_SEL_13;
	gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio_config(&gpio_cfg);

	char* buf = new char[80];

    gpio_set_level(GPIO_NUM_13, 1);
    BME280* bme = new BME280(i2c);
    bme280_reading_data data;
    bme->init();
    data = bme->readSensorData();
    gpio_set_level(GPIO_NUM_13, 0);

	oled.fill_rectangle(0, 0, 128, 64, BLACK);

	time_t t1 = time(NULL);
	tm* t2 = localtime(&t1);
	printf(asctime(t2));
	oled.draw_string(0, 0, asctime(t2), WHITE, BLACK);

	sprintf(buf, "%.1f *F %.0f%% RH %.2f inHg", data.temperature * 1.8 + 32, data.humidity, data.pressure / 3386.39);
	printf("%s\n",  buf);
	oled.draw_string(0, 26, buf, WHITE, BLACK);

	tcpip_adapter_ip_info_t ipInfo;
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
	sprintf(buf, "%d.%d.%d.%d bootcnt=%d", IP2STR(&ipInfo.ip), ++bootCount);
	oled.draw_string(0, 13, buf, WHITE, BLACK);

	report_client = esp_http_client_init(&report_config);
	sprintf(buf, "temp=%.2f&hum=%.2f&pres=%.2f",
		    data.temperature * 1.8 + 32, 
		    data.humidity, data.pressure / 3386.39);
    esp_http_client_set_post_field(report_client, buf, strlen(buf));
    if (esp_http_client_perform(report_client) == ESP_OK) {
        ESP_LOGI("http", "Status = %d, content_length = %d",
        esp_http_client_get_status_code(report_client),
        esp_http_client_get_content_length(report_client));
    }
    esp_http_client_cleanup(report_client);

    oled.refresh(true);

    esp_wifi_stop();

    esp_deep_sleep(5 * 60 * 1000000);
}
