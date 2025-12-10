/*******************************************************************************
 * @file        proyecto_3_embebidos.c
 * @brief       bucle principal de la aplicación que integra DHT11, LEDs,
 *              buzzer y comunicación con ThingsBoard vía MQTT.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#include "buzzer_utils.h"
#include "cJSON.h"
#include "dht11_utils.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_utils.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef WIFI_SSID
#error "WIFI_SSID must be defined using: -D WIFI_SSID=<YOUR_SSID>"
#endif

#ifndef WIFI_PASS
#error "WIFI_PASS must be defined using: -D WIFI_PASS=<YOUR_PASSWORD>"
#endif

// Configuración de ThingsBoard
#define THINGSBOARD_HOST "mqtt.thingsboard.cloud"
#define THINGSBOARD_PORT 1883
#define THINGSBOARD_ACCESS_TOKEN "EA7PtD7515SMcN240yJp"

esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *TAG = "DHT11_TB";
static bool mqtt_connected = false;

// Modo de control automático o manual
static bool automatic_mode = true;
static float last_temperature = 0.0;
static float last_humidity = 0.0;

// Manejo de eventos MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT Connected to ThingsBoard");
    mqtt_connected = true;
    esp_mqtt_client_subscribe(mqtt_client, "v1/devices/me/rpc/request/+", 1);
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT Disconnected");
    mqtt_connected = false;
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG, "MQTT Error");
    mqtt_connected = false;
    break;

  case MQTT_EVENT_DATA: {
    char topic[event->topic_len + 1];
    strncpy(topic, event->topic, event->topic_len);
    topic[event->topic_len] = 0;

    char payload[event->data_len + 1];
    strncpy(payload, event->data, event->data_len);
    payload[event->data_len] = 0;

    ESP_LOGI(TAG, "RPC topic: %s, payload: %s", topic, payload);

    cJSON *root = cJSON_Parse(payload);
    if (root) {
      const cJSON *method = cJSON_GetObjectItem(root, "method");
      const cJSON *params = cJSON_GetObjectItem(root, "params");

      // Establecer modo automático o manual
      if (method && params && strcmp(method->valuestring, "setMode") == 0) {
        const cJSON *mode = cJSON_GetObjectItem(params, "mode");
        if (mode && cJSON_IsString(mode)) {
          if (strcmp(mode->valuestring, "automatic") == 0) {
            automatic_mode = true;
            ESP_LOGI(TAG, "Mode set to: AUTOMATIC");
            led_reset_manual_override(1);
            led_reset_manual_override(2);
            led_reset_manual_override(3);
            if (last_humidity > 0) {
              ESP_LOGI(TAG, "Updating LEDs with last humidity: %.1f%%", last_humidity);
              leds_update_by_humidity(last_humidity);
            }
            if (last_temperature > 0) {
              ESP_LOGI(TAG, "Updating buzzer with last temperature: %.1f°C", last_temperature);
              buzzer_update_by_temperature(last_temperature);
            }
          } else if (strcmp(mode->valuestring, "manual") == 0) {
            automatic_mode = false;
            ESP_LOGI(TAG, "Mode set to: MANUAL");
          }
        }
      }

      // Control manual de LEDs
      else if (method && params && strcmp(method->valuestring, "setLED") == 0) {
        const cJSON *led = cJSON_GetObjectItem(params, "led");
        const cJSON *state = cJSON_GetObjectItem(params, "state");

        if (led && state) {
          int led_number = led->valueint;

          if (led_number < 1 || led_number > 3) {
            ESP_LOGW(TAG, "Invalid LED number: %d", led_number);
          } else {
            bool led_on =
                cJSON_IsBool(state) ? cJSON_IsTrue(state) : state->valueint;
            ESP_LOGI(TAG, "RPC request - LED%d -> %s", led_number,
                     led_on ? "ON" : "OFF");
            led_set(led_number, led_on, true); // manual = true
          }
        }
      }

      // Control manual del Buzzer
      else if (method && params &&
               strcmp(method->valuestring, "setBuzzer") == 0) {
        const cJSON *state = cJSON_GetObjectItem(params, "state");

        if (state) {
          bool buzzer_on =
              cJSON_IsBool(state) ? cJSON_IsTrue(state) : state->valueint;
          ESP_LOGI(TAG, "RPC request - Buzzer -> %s", buzzer_on ? "ON" : "OFF");
          buzzer_set(buzzer_on, true); // manual = true
        }
      }

      // Establecer umbral de temperatura para el Buzzer
      else if (method && params &&
               strcmp(method->valuestring, "setTempThreshold") == 0) {
        // Manejar tanto un número simple como un objeto json con "threshold"
        float new_threshold = 0.0;

        if (cJSON_IsNumber(params)) {
          new_threshold = (float)params->valuedouble;
        } else if (cJSON_IsObject(params)) {
          const cJSON *threshold = cJSON_GetObjectItem(params, "threshold");
          if (threshold && cJSON_IsNumber(threshold)) {
            new_threshold = (float)threshold->valuedouble;
          }
        }

        if (new_threshold > 0) {
          ESP_LOGI(TAG, "RPC request - Set temperature threshold to %.1f°C",
                   new_threshold);
          buzzer_set_threshold(new_threshold);
        } else {
          ESP_LOGW(TAG, "Invalid threshold value received");
        }
      }

      cJSON_Delete(root);
    }
    break;
  }

  default:
    break;
  }
}

// Manejo de eventos WiFi
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "Retrying WiFi connection...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

// Inicializar WiFi
static void wifi_init(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "WiFi initialization complete");
}

// Inicializar MQTT
static void mqtt_init(void) {
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.hostname = THINGSBOARD_HOST,
      .broker.address.port = THINGSBOARD_PORT,
      .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
      .credentials.username = THINGSBOARD_ACCESS_TOKEN,
  };

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);
}

// Enviar telemetría a ThingsBoard
static void send_telemetry(float temperature, float humidity) {
  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"temperature\":%.1f,\"humidity\":%.1f,\"buzzer\":%s,\"buzzer_"
           "mode\":\"%s\",\"temp_threshold\":%.1f,\"mode\":\"%s\"}",
           temperature, humidity, buzzer_get_state() ? "true" : "false",
           buzzer_is_manual_mode() ? "manual" : "auto", buzzer_get_threshold(),
           automatic_mode ? "automatic" : "manual");

  int msg_id = esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry",
                                       payload, 0, 1, 0);

  if (msg_id != -1) {
    ESP_LOGI(TAG, "Sent: %s", payload);
  } else {
    ESP_LOGE(TAG, "Failed to send telemetry");
  }
}

static void main_task(void *pvParameters) {
  float temperature, humidity;
  int retry_count = 0;
  const int MAX_RETRIES = 3;

  ESP_LOGI(TAG, "Sensor task started, waiting for MQTT connection...");

  while (!mqtt_connected) {
    ESP_LOGI(TAG, "Waiting for MQTT connection...");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  ESP_LOGI(TAG, "MQTT connected! Starting sensor readings...");

  while (1) {
    if (!mqtt_connected) {
      ESP_LOGW(TAG, "MQTT disconnected, waiting for reconnection...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    retry_count = 0;

    while (retry_count < MAX_RETRIES) {
      if (dht_read_data(&humidity, &temperature) == 0) {
        ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", temperature,
                 humidity);
        last_temperature = temperature;
        last_humidity = humidity;
        send_telemetry(temperature, humidity);
        break;
      } else {
        retry_count++;
        ESP_LOGW(TAG, "Read attempt %d/%d failed", retry_count, MAX_RETRIES);
        if (retry_count < MAX_RETRIES) {
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      }
    }

    // Actualizar salidas según el modo
    if (automatic_mode) {
      ESP_LOGI(TAG, "Running in AUTOMATIC mode");
      leds_update_by_humidity(humidity);
      buzzer_update_by_temperature(temperature);
    } else {
      ESP_LOGI(TAG, "Running in MANUAL mode");
    }

    if (retry_count >= MAX_RETRIES) {
      ESP_LOGE(TAG, "Failed to read DHT11 sensor after %d attempts",
               MAX_RETRIES);
    }

    vTaskDelay(pdMS_TO_TICKS(20000));
  }
}

void app_main(void) {
  ESP_LOGI(TAG, "Starting DHT11 ThingsBoard Application");

  // Inicializar NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_init();
  mqtt_init();

  gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

  leds_init();

  buzzer_init();

  // Ininiciar súper-tarea
  xTaskCreate(main_task, "main_task", 8192, NULL, 5, NULL);
}