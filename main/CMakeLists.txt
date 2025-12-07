cmake_minimum_required(VERSION 3.5)

# Inclui o caminho do arquivo ultrasonic.c conforme seu projeto original
#idf_component_register(SRCS "main.c" "http_server.c" "sensors_app.c" "wifi_app.c" "app_nvs.c" "../includes/ultrasonic.c"
                    #INCLUDE_DIRS "." "../includes"
                    #REQUIRES esp_adc esp_driver_gpio esp_timer nvs_flash esp_http_server esp_https_ota app_update esp_wifi lwip)

# Configura os arquivos fonte e recursos do componente
idf_component_register(SRCS "main.c"
                            "sensors_app.c" 
                            "sntp_time_sync.c"     # Adicionado para corrigir erro de tempo
                            "wifi_app.c"
                            "http_server.c"
                            "app_nvs.c"
                            "wifi_reset_button.c"
                            "../includes/ultrasonic.c" # Driver ultrassônico
                       
                       INCLUDE_DIRS "." "../includes"
                       
                       # ATENÇÃO: Seus arquivos web estão na pasta 'webpage/'
                       EMBED_FILES "webpage/index.html"
                                   "webpage/app.css"
                                   "webpage/app.js"
                                   "webpage/favicon.ico"
                                   "webpage/jquery-3.3.1.min.js"
                                   
                       REQUIRES esp_adc esp_driver_gpio esp_timer nvs_flash esp_http_server esp_https_ota app_update esp_wifi lwip esp_netif driver)