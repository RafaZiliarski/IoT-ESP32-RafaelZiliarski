#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "ultrasonic.h" // Certifique-se que este arquivo existe na pasta components ou includes

const static char *TAG = "MAIN_APP";

/*---------------------------------------------------------------
        Configurações - Pinos e Constantes
---------------------------------------------------------------*/
// LM35
#define LM35_CHANNEL          ADC_CHANNEL_5
#define EXAMPLE_ADC_ATTEN     ADC_ATTEN_DB_12
#define TEMP_ACIONAR          40.0       
#define TEMP_DESLIGAR         37.0

// Atuador (LED/Relé)
#define ACTUATOR_GPIO         GPIO_NUM_2 
#define PRESENCE_GPIO         GPIO_NUM_4

// Ultrassônico
#define MAX_DISTANCE_CM       500 // 5 metros
#define TRIGGER_GPIO          GPIO_NUM_5
#define ECHO_GPIO             GPIO_NUM_18

/*---------------------------------------------------------------
        Variáveis Globais (Handles)
---------------------------------------------------------------*/
// Handles do ADC precisam ser globais para serem vistos pela task e inicializados no main
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

/*---------------------------------------------------------------
        Protótipos de Funções
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

// Protótipos das Tasks
void task_lm35(void *pvParameters);
void task_ultrasonic(void *pvParameters);

/*---------------------------------------------------------------
        APP MAIN (Configuração e Criação das Tasks)
---------------------------------------------------------------*/
void app_main(void)
{
    // 1. Configuração da GPIO do Atuador
    gpio_reset_pin(ACTUATOR_GPIO);
    gpio_reset_pin(PRESENCE_GPIO);
    gpio_set_direction(ACTUATOR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(PRESENCE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(ACTUATOR_GPIO, 0); 
    gpio_set_level(PRESENCE_GPIO, 0);

    // 2. Configuração do ADC (LM35)
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LM35_CHANNEL, &config));

    // 3. Calibração do ADC
    do_calibration = example_adc_calibration_init(ADC_UNIT_1, LM35_CHANNEL, EXAMPLE_ADC_ATTEN, &adc1_cali_handle);

    // 4. Criação das Tasks (Multitarefa)
    
    // Cria a Task do LM35 (Stack de 2048 bytes, Prioridade 5)
    xTaskCreate(task_lm35, "LM35_Task", 2048, NULL, 5, NULL);

    // Cria a Task do Ultrassônico (Stack de 2048 bytes, Prioridade 5)
    xTaskCreate(task_ultrasonic, "Ultrasonic_Task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sistema iniciado. Tasks rodando...");
}

/*---------------------------------------------------------------
        TASK 1: Sensor de Temperatura (LM35)
---------------------------------------------------------------*/
void task_lm35(void *pvParameters)
{
    int adc_raw;
    int voltage;
    bool estado_atuador = false;

    while (1) {
        // Leitura Raw
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LM35_CHANNEL, &adc_raw));
        
        if (do_calibration) {
            // Converte Raw -> Voltagem (mV)
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
            
            // Converte Voltagem -> Temperatura (LM35: 10mV por °C)
            float temperature = (float)voltage / 10.0;
            
            // Log apenas informativo
            // ESP_LOGI("LM35", "Temp: %.2f C", temperature);

            // --- LÓGICA DE HISTERESE ---
            if ((temperature >= TEMP_ACIONAR) && (estado_atuador == false)) {
                estado_atuador = true;
                ESP_LOGW("LM35", "Temp %.2f C >= %.1f -> LIGANDO ATUADOR", temperature, TEMP_ACIONAR);
                gpio_set_level(ACTUATOR_GPIO, 1);
            } 
            else if ((temperature <= TEMP_DESLIGAR) && (estado_atuador == true)) {
                estado_atuador = false;
                ESP_LOGW("LM35", "Temp %.2f C <= %.1f -> DESLIGANDO ATUADOR", temperature, TEMP_DESLIGAR);
                gpio_set_level(ACTUATOR_GPIO, 0);
            }
        } else {
            ESP_LOGE("LM35", "Erro de calibracao");
        }

        // Delay de 1 segundo para não saturar a CPU
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*---------------------------------------------------------------
        TASK 2: Sensor Ultrassônico (HC-SR04)
---------------------------------------------------------------*/
void task_ultrasonic(void *pvParameters)
{
    bool estado_presenca = false;

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };

    ultrasonic_init(&sensor);

    while (true)
    {
        float distance;
        // Mede a distância
        esp_err_t res = ultrasonic_measure(&sensor, MAX_DISTANCE_CM, &distance);
        
        if (res != ESP_OK)
        {
            // Tratamento de erros simplificado para não poluir o log
            // ESP_LOGE("ULTRASONIC", "Error %d: %s", res, esp_err_to_name(res));
        }
        else
        {
            // A biblioteca geralmente retorna em Metros. Multiplicamos por 100 para CM.
            float distance_cm = distance * 100;
            if (distance_cm < 50.0 && !estado_presenca)
            {
                estado_presenca = true;
                ESP_LOGW("ULTRASONIC", "Presença detectada a %.2f cm!", distance_cm);
                // Aqui você pode adicionar ações adicionais, como acionar um alarme ou notificação
            }
            else if (distance_cm >= 50.0 && estado_presenca)
            {
                estado_presenca = false;
                ESP_LOGI("ULTRASONIC", "Área livre. Nenhuma presença detectada.");
            }
            ESP_LOGI("ULTRASONIC", "Distancia: %.2f cm", distance_cm);
        }
        gpio_set_level(PRESENCE_GPIO, estado_presenca ? 1 : 0);
        // Delay de 500ms entre leituras
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*---------------------------------------------------------------
        Funções Auxiliares de Calibração (Boilerplate)
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}