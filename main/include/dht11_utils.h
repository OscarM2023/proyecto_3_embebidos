/*******************************************************************************
 * @file        dht11_utils.h
 * @brief       Funciones para interactuar con el sensor DHT11.
 * @author      Nagel Mejía Segura, Wilberth Gutiérrez Montero, Óscar González Cambronero
 * @date        30/8/2025
 * @version     1.0.0
 *
 ******************************************************************************/

#ifndef DHT11_UTILS_H
#define DHT11_UTILS_H

#include "stdint.h"

#define DHT_GPIO GPIO_NUM_23
#define DHT_TIMEOUT 200

int dht_await_pin_state(uint32_t timeout, int expected_state);
int dht_read_data(float *humidity, float *temperature);

#endif // DHT11_UTILS_H