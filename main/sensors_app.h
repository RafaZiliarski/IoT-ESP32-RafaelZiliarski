/*
 * sensors_app.h
 *
 * Cabeçalho para gerenciar LM35 e Ultrassônico
 */

#ifndef SENSORS_APP_H_
#define SENSORS_APP_H_

#include <stdbool.h>

// Configurações de Pinos
#define LM35_CHANNEL          ADC_CHANNEL_5 
#define ACTUATOR_GPIO         GPIO_NUM_2 
#define PRESENCE_GPIO         GPIO_NUM_4 
#define TRIGGER_GPIO          GPIO_NUM_5
#define ECHO_GPIO             GPIO_NUM_18

/**
 * Inicializa os sensores e inicia as Tasks do FreeRTOS
 */
void sensors_app_start(void);

/**
 * Retorna a última temperatura lida (em Graus Celsius)
 */
float sensors_get_temp(void);

/**
 * Retorna a última distância lida (em cm)
 */
float sensors_get_distance(void);

/**
 * Retorna o estado do atuador (true = LIGADO, false = DESLIGADO)
 */
bool sensors_get_actuator_status(void);

float sensors_get_cooling_power(void);

#endif /* SENSORS_APP_H_ */