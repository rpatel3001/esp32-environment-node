/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
extern "C" {
	#include "ssd1306.h"
	#include "fonts.h"
}
#include "BME280.h"

extern "C" {
   void app_main();
}

void app_main()
{
	char* buf = new char[80];

	BME280* sensor = new BME280();
	sensor->init(GPIO_NUM_5, GPIO_NUM_4);
	bme280_reading_data data;

	if(!ssd1306_init(0, 4, 5)) {
		ESP_LOGI("OLED", "init failed");
	} else {
		ESP_LOGI("OLED", "init done");
	}

	data = sensor->readSensorData();

	while(1) {
		data = sensor->readSensorData();
		data = sensor->readSensorData();

		ssd1306_init(0, 4, 5);

		sprintf(buf, "%.1f deg F", data.temperature * 9 / 5 + 32);
		ESP_LOGI("BME", "%s",  buf);
		ssd1306_draw_string(0, 0, 0, buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

		sprintf(buf, "%.1f%% RH", data.humidity);
		ESP_LOGI("BME", "%s", buf);
		ssd1306_draw_string(0, 0, 20, buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

		sprintf(buf, "%.1f kPa\n", data.pressure / 1000);
		ESP_LOGI("BME", "%s",  buf);
		ssd1306_draw_string(0, 0, 40, buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

		ssd1306_refresh(0, true);
	}
}
