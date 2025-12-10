/*******************************************************************************
 * @file        buzzer_utils.c
 * @brief       Funciones para controlar el buzzer basado en el umbral de temperatura.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#include "buzzer_utils.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BUZZER";

static bool buzzer_state = false;
static bool manual_mode = false;
static float temp_threshold = TEMP_THRESHOLD;
esp_err_t buzzer_init(void) {
  esp_err_t ret;

  // Configurar el timer LEDC
  ledc_timer_config_t ledc_timer = {.speed_mode = BUZZER_LEDC_MODE,
                                    .duty_resolution = BUZZER_LEDC_DUTY_RES,
                                    .timer_num = BUZZER_LEDC_TIMER,
                                    .freq_hz = BUZZER_FREQUENCY,
                                    .clk_cfg = LEDC_AUTO_CLK};

  ret = ledc_timer_config(&ledc_timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
    return ret;
  }

  ledc_channel_config_t ledc_channel = {.gpio_num = BUZZER_GPIO,
                                        .speed_mode = BUZZER_LEDC_MODE,
                                        .channel = BUZZER_LEDC_CHANNEL,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .timer_sel = BUZZER_LEDC_TIMER,
                                        .duty = 0, // Iniciar con buzzer apagado
                                        .hpoint = 0};

  ret = ledc_channel_config(&ledc_channel);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "Buzzer initialized on GPIO%d at %dHz", BUZZER_GPIO,
           BUZZER_FREQUENCY);
  ESP_LOGI(TAG, "Temperature threshold: %.1f°C", temp_threshold);

  return ESP_OK;
}

void buzzer_set(bool state, bool manual) {
  if (manual) {
    manual_mode = true;
    ESP_LOGI(TAG, "Manual mode activated");
  }

  buzzer_state = state;

  // Establecer ciclo de trabajo
  uint32_t duty = state ? BUZZER_LEDC_DUTY : 0;

  ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, duty);
  ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);

  ESP_LOGI(TAG, "Buzzer %s %s", state ? "ON" : "OFF",
           manual ? "(manual)" : "(auto)");
}

void buzzer_update_by_temperature(float temperature) {
  // No actualizar si está en modo manual
  if (manual_mode) {
    return;
  }

  bool should_activate = temperature > temp_threshold;

  // Solo cambiar estado si es diferente
  if (should_activate != buzzer_state) {
    buzzer_set(should_activate, false);

    if (should_activate) {
      ESP_LOGW(TAG,
               "Temperature %.1f°C exceeds threshold %.1f°C - Buzzer activated",
               temperature, temp_threshold);
    } else {
      ESP_LOGI(TAG,
               "Temperature %.1f°C below threshold %.1f°C - Buzzer deactivated",
               temperature, temp_threshold);
    }
  }
}

bool buzzer_get_state(void) { return buzzer_state; }

bool buzzer_is_manual_mode(void) { return manual_mode; }

void buzzer_set_threshold(float threshold) {
  temp_threshold = threshold;
  ESP_LOGI(TAG, "Temperature threshold updated to %.1f°C", threshold);

  // Resetear modo manual al cambiar el umbral
  manual_mode = false;
  ESP_LOGI(TAG, "Switched to automatic mode");
}

float buzzer_get_threshold(void) { return temp_threshold; }