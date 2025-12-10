/*******************************************************************************
 * @file        dht11_utils.c
 * @brief       Funciones para interactuar con el sensor DHT11.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#include "dht11_utils.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>

const char *TAG = "DHT11_UTILS";

// spinlock para secciones críticas
static portMUX_TYPE dht_spinlock = portMUX_INITIALIZER_UNLOCKED;

int dht_await_pin_state(uint32_t timeout, int expected_state) {
  for (uint32_t i = 0; i < timeout; i++) {
    if (gpio_get_level(DHT_GPIO) == expected_state)
      return 0;
    esp_rom_delay_us(1);
  }
  return -1;
}

int dht_read_data(float *humidity, float *temperature) {
  uint8_t data[5] = {0};
  int result = 0;

  // Enviar señal de inicio, puede ser interrumpida
  gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(DHT_GPIO, 0);
  esp_rom_delay_us(20000); // 20ms LOW

  gpio_set_level(DHT_GPIO, 1);
  esp_rom_delay_us(40); // 20-40us HIGH

  // Cambiar a entrada para leer datos
  gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);

  portENTER_CRITICAL(&dht_spinlock);

  // Esperar respuesta del DHT11 bajar a LOW - 80us
  if (dht_await_pin_state(DHT_TIMEOUT, 0) != 0) {
    result = -1;
    goto cleanup;
  }

  // Esperar a que DHT11 suba a HIGH - 80us
  if (dht_await_pin_state(DHT_TIMEOUT, 1) != 0) {
    result = -1;
    goto cleanup;
  }

  // Esperar a que DHT11 baje a LOW - inicio de datos
  if (dht_await_pin_state(DHT_TIMEOUT, 0) != 0) {
    result = -1;
    goto cleanup;
  }

  // Leer 40 bits
  for (int i = 0; i < 40; i++) {
    // Esperar a que suba a HIGH - inicio del bit
    // Esperar a la señal en alto que indica el valor del bit
    if (dht_await_pin_state(DHT_TIMEOUT, 1) != 0) {
      result = -1;
      goto cleanup;
    }

    // Crítico: medir duración del pulso HIGH
    esp_rom_delay_us(30);

    if (gpio_get_level(DHT_GPIO)) {
      // Todavía está HIGH, es un bit 1
      data[i / 8] |= (1 << (7 - (i % 8)));
    }

    // Esperar a que baje a LOW - fin del bit
    if (dht_await_pin_state(DHT_TIMEOUT, 0) != 0) {
      result = -1;
      goto cleanup;
    }
  }

cleanup:
  // Siempre salir de la sección crítica
  portEXIT_CRITICAL(&dht_spinlock);

  // Si hubo un error durante la lectura, retornar
  if (result != 0) {
    ESP_LOGE(TAG, "DHT11 read failed");
    return result;
  }

  // Verificar checksum
  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    ESP_LOGE(TAG, "Checksum error: expected %d, got %d",
             ((data[0] + data[1] + data[2] + data[3]) & 0xFF), data[4]);
    return -1;
  }

  *humidity = data[0];
  *temperature = data[2];

  ESP_LOGI(TAG, "Raw data: %d %d %d %d %d", data[0], data[1], data[2], data[3],
           data[4]);

  return 0;
}