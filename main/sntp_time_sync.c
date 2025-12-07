/*
 * sntp_time_sync.c
 * Atualizado para ESP-IDF 5.x
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sntp.h" // Biblioteca correta para v5.x
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tasks_common.h"
#include "http_server.h"
#include "sntp_time_sync.h"
#include "wifi_app.h"

static const char TAG[] = "sntp_time_sync";

// Flag para saber se o modo de operação já foi setado
static bool sntp_op_mode_set = false;

/**
 * Inicializa o serviço SNTP
 */
static void sntp_time_sync_init_sntp(void)
{
    ESP_LOGI(TAG, "Inicializando servico SNTP...");

    if (!sntp_op_mode_set)
    {
        // Configura para modo Polling (consulta periódica)
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_op_mode_set = true;
    }

    // Define o servidor NTP (pool global)
    esp_sntp_setservername(0, "pool.ntp.org");

    // Inicia o serviço
    esp_sntp_init();

    // Avisa o monitor do servidor HTTP que o serviço de tempo iniciou
    // Isso libera o envio da hora no JSON /localTime.json
    http_server_monitor_send_message(HTTP_MSG_TIME_SERVICE_INITIALIZED);
}

/**
 * Verifica a hora atual e inicia o SNTP se necessário
 */
static void sntp_time_sync_obtain_time(void)
{
    time_t now = 0;
    struct tm time_info = {0};

    time(&now);
    localtime_r(&now, &time_info);

    // Se o ano for menor que 2016, o tempo não está ajustado
    if (time_info.tm_year < (2016 - 1900))
    {
        sntp_time_sync_init_sntp();
        
        // Define o Fuso Horário para Brasília (BRT/BRST)
        // Referência: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
        //setenv("TZ", "BRT3BRST,M10.3.0/0,M2.3.0/0", 1);
		setenv("TZ", "BRT3", 1);
        tzset();
    }
}

/**
 * A Task principal de sincronização
 */
static void sntp_time_sync_task(void *pvParam)
{
    while (1)
    {
        // Verifica se precisa sincronizar
        sntp_time_sync_obtain_time();
        
        // Aguarda 10 segundos antes de checar novamente
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    vTaskDelete(NULL);
}

/**
 * Função chamada pelo servidor web para pegar a string de hora
 */
char* sntp_time_sync_get_time(void)
{
    static char time_buffer[100] = {0};

    time_t now = 0;
    struct tm time_info = {0};

    time(&now);
    localtime_r(&now, &time_info);

    if (time_info.tm_year < (2016 - 1900))
    {
        ESP_LOGI(TAG, "Hora ainda nao ajustada");
        strcpy(time_buffer, "Aguardando Sync...");
    }
    else
    {
        // Formata: DD/MM/YYYY HH:MM:SS
        strftime(time_buffer, sizeof(time_buffer), "%d/%m/%Y %H:%M:%S", &time_info);
    }

    return time_buffer;
}

/**
 * Inicia a Task (Deve ser chamada no main.c)
 */
void sntp_time_sync_task_start(void)
{
    // AQUI ESTAVA O ERRO: O nome da função é sntp_time_sync_task
    xTaskCreatePinnedToCore(sntp_time_sync_task, "sntp_time_sync", 
                            SNTP_TIME_SYNC_TASK_STACK_SIZE, NULL, 
                            SNTP_TIME_SYNC_TASK_PRIORITY, NULL, 
                            SNTP_TIME_SYNC_TASK_CORE_ID);
}