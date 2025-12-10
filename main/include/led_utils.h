/*******************************************************************************
 * @file        led_utils.h
 * @brief       Funciones para controlar los LEDs basados en el umbral de humedad.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#ifndef LED_UTILS_H
#define LED_UTILS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t gpio;
  float humidity_threshold;
  bool state;
  bool manual_override;
} led_t;

void leds_init(void);
void led_set(uint8_t led_number, bool on, bool manual);
void leds_update_by_humidity(float humidity);
void led_reset_manual_override(uint8_t led_number);

#endif // LED_UTILS_H
