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
#include "I2C.h"
#include "BME280.h"
#include "ssd1306.hpp"

extern "C" {
   void app_main();
}

void app_main()
{
	I2C i2c = I2C();
	i2c.init(GPIO_NUM_5, GPIO_NUM_4);

	char* buf = new char[80];

	OLED oled = OLED(&i2c, SSD1306_128x64);
	oled.init();
	oled.select_font(1);

	BME280* bme = new BME280();
	bme->init(i2c);
	bme280_reading_data data;

	while(1) {
		data = bme->readSensorData();

		sprintf(buf, "%.2f deg", data.temperature * 9 / 5 + 32);
		printf("%s\t",  buf);
		oled.draw_string(0, 0, buf, WHITE, BLACK);

		sprintf(buf, "%.2f%% RH", data.humidity);
		printf("%s\t", buf);
		oled.draw_string(0, 15, buf, WHITE, BLACK);

		sprintf(buf, "%.2f kPa", data.pressure / 1000);
		printf("%s\n",  buf);
		oled.draw_string(0, 30, buf, WHITE, BLACK);

    	oled.refresh(true);
		oled.fill_rectangle(0, 0, 128, 64, BLACK);
	    vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}
