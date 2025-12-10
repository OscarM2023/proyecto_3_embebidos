/*******************************************************************************
 * @file        buzzer_utils.h
 * @brief       Funciones para controlar el buzzer basado en el umbral de temperatura.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#ifndef BUZZER_UTILS_H
#define BUZZER_UTILS_H

#include "esp_err.h"
#include <stdbool.h>

#define BUZZER_GPIO GPIO_NUM_33

#ifndef BUZZER_FREQUENCY
#define BUZZER_FREQUENCY 2000
#endif

#define BUZZER_LEDC_TIMER LEDC_TIMER_1
#define BUZZER_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL LEDC_CHANNEL_1
#define BUZZER_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define BUZZER_LEDC_DUTY 512 // 50% ciclo de trabajo para 10 bits

#ifndef TEMP_THRESHOLD
#define TEMP_THRESHOLD 30.0
#endif

esp_err_t buzzer_init(void);
void buzzer_set(bool state, bool manual);
void buzzer_update_by_temperature(float temperature);
bool buzzer_get_state(void);
bool buzzer_is_manual_mode(void);
void buzzer_set_threshold(float threshold);
float buzzer_get_threshold(void);

#endif // BUZZER_UTILS_H