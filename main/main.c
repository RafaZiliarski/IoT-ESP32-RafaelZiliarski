#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_nvs.h" 
#include "wifi_app.h"
#include "http_server.h" 
#include "sensors_app.h" 
#include "sntp_time_sync.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    // ====================================================
    // 1. Inicializa NVS (Non-Volatile Storage)
    // ====================================================
    // Isso é obrigatório para o WiFi funcionar e salvar senhas
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ====================================================
    // 2. Inicia a Aplicação WiFi
    // ====================================================
    ESP_LOGI(TAG, "Iniciando WiFi...");
    wifi_app_start();

    ESP_LOGI(TAG, "Iniciando SNTP...");
    sntp_time_sync_task_start();

    ESP_LOGI(TAG, "Iniciando Web Server...");
    http_server_start();

    // ====================================================
    // 4. Inicia os Sensores (LM35 + Ultrassônico + Atuador)
    // ====================================================
    // Toda a configuração de GPIOs, ADC e Tasks está dentro desta função
    // no arquivo sensors_app.c
    ESP_LOGI(TAG, "Iniciando Sensores e Tasks...");
    sensors_app_start();
    
    ESP_LOGI(TAG, "Sistema Completo Iniciado.");
}