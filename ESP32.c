// Inclusão de bibliotecas necessárias
#include <stdio.h>             // Funções de entrada/saída padrão
#include <string.h>            // Funções de manipulação de strings
#include "freertos/FreeRTOS.h" // Sistema operacional em tempo real
#include "freertos/task.h"     // Gerenciamento de tarefas do FreeRTOS
#include "esp_log.h"           // Sistema de logging do ESP32
#include "esp_bt.h"            // Controlador Bluetooth do ESP32
#include "esp_gap_ble_api.h"   // API GAP (Generic Access Profile) para BLE
#include "esp_bt_main.h"       // Inicialização principal do Bluetooth
#include "esp_bt_device.h"     // Funções do dispositivo Bluetooth

// Tag para identificar logs deste módulo
static const char *TAG = "BLE_FAKE";

// Array de nomes simulados para os dispositivos BLE
// Estes nomes aparecerão quando o dispositivo for escaneado
char *device_names[] = {"DISP_01", "DISP_02", "DISP_03"};

// Array de endereços MAC simulados para cada dispositivo
// Cada dispositivo terá um MAC único para identificação
uint8_t device_macs[][6] = {
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}, // MAC do DISP_01
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02}, // MAC do DISP_02
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x03}  // MAC do DISP_03
};

// Índice atual para rotacionar entre os dispositivos simulados
int current_idx = 0;

// Parâmetros de configuração para o advertising BLE
esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,                                    // Intervalo mínimo de advertising (32 * 0.625ms = 20ms)
    .adv_int_max = 0x40,                                    // Intervalo máximo de advertising (64 * 0.625ms = 40ms)
    .adv_type = ADV_TYPE_IND,                               // Tipo: connectable undirected advertising
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,                  // Usar endereço MAC aleatório
    .channel_map = ADV_CHNL_ALL,                            // Usar todos os canais de advertising (37, 38, 39)
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY, // Permitir scan e conexão de qualquer dispositivo
};

/**
 * Função para configurar e iniciar o advertising BLE com um nome específico
 * @param name Nome do dispositivo a ser anunciado
 */
void start_advertising(const char *name)
{
    // Buffer para armazenar os dados de advertising (máximo 31 bytes)
    uint8_t adv_data[31];
    memset(adv_data, 0, sizeof(adv_data)); // Limpa o buffer
    int i = 0;                             // Índice para construir o pacote de dados

    // Adiciona o campo Flags (obrigatório no advertising)
    adv_data[i++] = 2;    // Comprimento do campo Flags (2 bytes)
    adv_data[i++] = 0x01; // Tipo: Flags
    adv_data[i++] = 0x06; // Valor: LE General Discoverable + BR/EDR Not Supported

    // Calcula o tamanho do nome (limitado a 29 bytes para caber no pacote)
    size_t name_len = strlen(name);
    if (name_len > 29)
        name_len = 29; // Limita o tamanho do nome

    // Adiciona o campo Complete Local Name
    adv_data[i++] = name_len + 1;         // Comprimento do campo (nome + tipo)
    adv_data[i++] = 0x09;                 // Tipo: Complete Local Name
    memcpy(&adv_data[i], name, name_len); // Copia o nome
    i += name_len;

    // Configura os dados de advertising no controlador BLE
    esp_ble_gap_config_adv_data_raw(adv_data, i);
}

/**
 * Tarefa principal que executa o loop de advertising
 * Alterna entre os diferentes dispositivos simulados a cada 2 segundos
 * @param param Parâmetro da tarefa (não utilizado)
 */
void advertising_loop_task(void *param)
{
    while (1)
    {
        // Log informativo mostrando qual dispositivo está sendo anunciado
        ESP_LOGI(TAG, ">> Device: %s, MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 device_names[current_idx],
                 device_macs[current_idx][0], device_macs[current_idx][1],
                 device_macs[current_idx][2], device_macs[current_idx][3],
                 device_macs[current_idx][4], device_macs[current_idx][5]);

        // Para o advertising atual (se estiver ativo)
        esp_ble_gap_stop_advertising();

        // Define o endereço MAC aleatório para o dispositivo atual
        esp_ble_gap_set_rand_addr(device_macs[current_idx]);

        // Configura os dados de advertising com o nome do dispositivo atual
        start_advertising(device_names[current_idx]);

        // Inicia o advertising com os parâmetros configurados
        esp_ble_gap_start_advertising(&adv_params);

        // Avança para o próximo dispositivo (rotação circular)
        current_idx = (current_idx + 1) % 3;

        // Aguarda 2 segundos antes de trocar para o próximo dispositivo
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * Função de callback para eventos GAP (Generic Access Profile) do BLE
 * Processa eventos relacionados ao advertising
 * @param event Tipo do evento GAP
 * @param param Parâmetros do evento
 */
void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    // Evento disparado quando os dados de advertising são configurados
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT)
    {
        ESP_LOGI(TAG, "Advertising data set"); // Log de confirmação
    }
    // Evento disparado quando o advertising é iniciado
    else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT)
    {
        // Verifica se o advertising foi iniciado com sucesso
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Failed to start advertising"); // Log de erro
        }
        else
        {
            ESP_LOGI(TAG, "Advertising started"); // Log de sucesso
        }
    }
}

/**
 * Função principal da aplicação
 * Inicializa o sistema Bluetooth e cria a tarefa de advertising
 */
void app_main(void)
{
    // Configuração padrão do controlador Bluetooth
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    // Inicializa o controlador Bluetooth com a configuração
    esp_bt_controller_init(&bt_cfg);

    // Habilita o controlador em modo BLE (Bluetooth Low Energy)
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    // Inicializa a stack Bluedroid (implementação Bluetooth do ESP32)
    esp_bluedroid_init();

    // Habilita a stack Bluedroid
    esp_bluedroid_enable();

    // Registra a função de callback para eventos GAP
    esp_ble_gap_register_callback(gap_cb);

    // Cria uma tarefa para executar o loop de advertising
    // Parâmetros: função, nome, tamanho da stack, parâmetro, prioridade, handle
    xTaskCreate(advertising_loop_task, "ble_loop", 4096, NULL, 5, NULL);
}
