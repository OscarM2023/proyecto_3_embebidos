/*******************************************************************************
 * @file        led_utils.c
 * @brief       Funciones para controlar los LEDs basados en el umbral de humedad.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#include "led_utils.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mqtt_client.h"

static led_t leds[3] = {
    {GPIO_NUM_5, 50.0, false, false},
    {GPIO_NUM_22, 65.0, false, false},
    {GPIO_NUM_21, 80.0, false, false},
};

static const char *TAG = "LED_CONTROL";
extern esp_mqtt_client_handle_t mqtt_client;

// Inicializar los LEDs
void leds_init(void) {
  gpio_config_t io_conf = {
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  io_conf.pin_bit_mask =
      (1ULL << leds[0].gpio) | (1ULL << leds[1].gpio) | (1ULL << leds[2].gpio);
  gpio_config(&io_conf);

  for (int i = 0; i < 3; i++) {
    gpio_set_level(leds[i].gpio, 0);
    leds[i].state = false;
    leds[i].manual_override = false;
  }
}

// Cambiar el estado de un LED específico
void led_set(uint8_t led_number, bool on, bool manual) {
  if (led_number < 1 || led_number > 3)
    return;

  led_t *led = &leds[led_number - 1];

  if (manual) {
    led->manual_override = true;
  }

  gpio_set_level(led->gpio, on ? 1 : 0);
  led->state = on;

  ESP_LOGI(TAG, "LED %d turned %s %s", led_number, on ? "ON" : "OFF",
           manual ? "(manual)" : "(auto)");

  // Publicar el estado del LED a través de MQTT
  if (mqtt_client != NULL) {
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"led%d\":%d}", led_number,
             on ? 1 : 0);
    esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", payload, 0,
                            1, 0);
  }
}

// Automáticamente actualizar los LEDs según la humedad
void leds_update_by_humidity(float humidity) {
  for (int i = 0; i < 3; i++) {
    led_t *led = &leds[i];
    if (!led->manual_override) {
      bool should_turn_on = humidity > led->humidity_threshold;
      if (led->state != should_turn_on) {
        led_set(i + 1, should_turn_on, false); // manual = false
      }
    }
  }
}

// Reinicio manual de un LED específico
void led_reset_manual_override(uint8_t led_number) {
  if (led_number < 1 || led_number > 3)
    return;
  leds[led_number - 1].manual_override = false;
}
