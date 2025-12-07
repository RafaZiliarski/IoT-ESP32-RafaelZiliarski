/*
 * sensors_app.c
 */

#include "tasks_common.h"
#include "sensors_app.h"
#include "ultrasonic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/ledc.h"

static const char *TAG = "SENSORS_APP";

// Configurações do LM35
#define EXAMPLE_ADC_ATTEN     ADC_ATTEN_DB_12
#define TEMP_ACIONAR          40.0       
#define TEMP_DESLIGAR         37.0
#define MAX_DISTANCE_CM       400 // 4 metros

// Variáveis Globais
static float g_current_temp = 0.0;
static float g_current_distance = 0.0;
static bool g_actuator_state = false;
static bool g_presence_state = false;

// PWM duty atual
static int current_duty = 0;

// Handles do ADC
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

// Protótipos locais
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
void task_lm35(void *pvParameters);
void task_ultrasonic(void *pvParameters);
void task_pwm(void *pvParameters);
void pwm_init(void);
void pwm_set_duty(uint32_t duty);

// --- Getters ---
float sensors_get_temp(void) { return g_current_temp; }
float sensors_get_distance(void) { return g_current_distance; }
bool sensors_get_actuator_status(void) { return g_actuator_state; }
float sensors_get_cooling_power(void) { return ((float)current_duty / 8191.0f) * 100.0f; }

// --- Inicialização ---
void sensors_app_start(void) {
    // GPIOs de atuador e presença
    gpio_reset_pin(ACTUATOR_GPIO);
    gpio_reset_pin(PRESENCE_GPIO);
    gpio_set_direction(ACTUATOR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(PRESENCE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(ACTUATOR_GPIO, 0);
    gpio_set_level(PRESENCE_GPIO, 0);

    // ADC LM35
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LM35_CHANNEL, &config));
    do_calibration = adc_calibration_init(ADC_UNIT_1, LM35_CHANNEL, EXAMPLE_ADC_ATTEN, &adc1_cali_handle);

    // Inicializa PWM
    pwm_init();

    // Cria tasks
    xTaskCreatePinnedToCore(task_lm35, "task_lm35", LM35_TASK_STACK_SIZE, NULL, LM35_TASK_PRIORITY, NULL, LM35_TASK_CORE_ID);
    xTaskCreatePinnedToCore(task_ultrasonic, "task_ultrasonic", ULTRASONIC_TASK_STACK_SIZE, NULL, ULTRASONIC_TASK_PRIORITY, NULL, ULTRASONIC_TASK_CORE_ID);
    xTaskCreatePinnedToCore(task_pwm, "task_pwm", 2048, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "Sensores + PWM Iniciados");
}

// --- Task LM35 ---
void task_lm35(void *pvParameters) {
    int adc_raw, voltage;
    while (1) {
        if (adc_oneshot_read(adc1_handle, LM35_CHANNEL, &adc_raw) == ESP_OK) {
            if (do_calibration) {
                adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage);
                float temp = (float)voltage / 10.0;
                g_current_temp = temp;

                if ((temp >= TEMP_ACIONAR) && !g_actuator_state) {
                    g_actuator_state = true;
                    ESP_LOGW(TAG, "Temp alta (%.1f). Atuador LIGADO.", temp);
                } else if ((temp <= TEMP_DESLIGAR) && g_actuator_state) {
                    g_actuator_state = false;
                    ESP_LOGW(TAG, "Temp normal (%.1f). Atuador DESLIGADO.", temp);
                }
            }
        }
        gpio_set_level(ACTUATOR_GPIO, g_actuator_state ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- Task Ultrassônico ---
void task_ultrasonic(void *pvParameters) {
    ultrasonic_sensor_t sensor = { .trigger_pin = TRIGGER_GPIO, .echo_pin = ECHO_GPIO };
    ultrasonic_init(&sensor);

    while (1) {
        float distance_meters;
        if (ultrasonic_measure(&sensor, MAX_DISTANCE_CM/100.0, &distance_meters) == ESP_OK) {
            g_current_distance = distance_meters * 100.0;
            g_presence_state = (g_current_distance < 50.0);
            gpio_set_level(PRESENCE_GPIO, g_presence_state ? 1 : 0);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// --- Task PWM (duty proporcional à temperatura) ---
void task_pwm(void *pvParameters) {
    const int duty_max = 8191; // 13 bits
    const float temp_min = 25.0;
    const float temp_max = 50.0;

    while (1) {
        float temp = g_current_temp;

        int duty = 0;
        if (temp > temp_min) {
            duty = (int)(((temp - temp_min) / (temp_max - temp_min)) * duty_max);
            if (duty > duty_max) duty = duty_max;
        }

        pwm_set_duty(duty);
        ESP_LOGI("PWM", "Temp: %.1f °C -> Duty: %d (%.1f%%)", temp, duty, sensors_get_cooling_power());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- PWM ---
void pwm_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 500,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = 32,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG, "PWM inicializado no GPIO32");
}

void pwm_set_duty(uint32_t duty) {
    current_duty = duty;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
}

// --- Calibração ADC ---
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
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
        if (ret == ESP_OK) calibrated = true;
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
        if (ret == ESP_OK) calibrated = true;
    }
#endif
    *out_handle = handle;
    return calibrated;
}
