// Inclusão de bibliotecas necessárias
#include <stdio.h>               // Funções de entrada/saída padrão
#include <stdlib.h>              // Funções de controle de programa (exit, malloc, etc.)
#include <unistd.h>              // Funções do sistema UNIX (sleep, close, etc.)
#include <sys/socket.h>          // Funções de socket para comunicação de rede
#include <sys/ioctl.h>           // Funções de controle de dispositivos (ioctl)
#include <bluetooth/bluetooth.h> // Definições básicas do Bluetooth (bdaddr_t, str2ba, etc.)
#include <bluetooth/hci.h>       // Interface HCI (Host Controller Interface) do Bluetooth
#include <bluetooth/hci_lib.h>   // Biblioteca de funções HCI de alto nível
#include <string.h>              // Funções de manipulação de strings (strlen, memcpy, etc.)

/**
 * Define um endereço MAC aleatório para o dispositivo Bluetooth
 * Esta função é essencial para simular diferentes dispositivos, pois cada
 * dispositivo BLE deve ter um endereço MAC único para identificação
 * @param sock: socket HCI do dispositivo Bluetooth
 * @param mac: endereço MAC a ser configurado (formato bdaddr_t)
 */
void set_random_mac(int sock, bdaddr_t mac)
{
    // Estrutura de comando para enviar o MAC via HCI
    // O atributo packed garante que não haja padding entre os campos
    struct
    {
        uint8_t bdaddr[6]; // Array de 6 bytes para o endereço MAC (formato padrão)
    } __attribute__((packed)) cmd;

    // Copia o endereço MAC para a estrutura de comando
    memcpy(cmd.bdaddr, &mac, 6);

    // Envia comando HCI para definir endereço aleatório no controlador Bluetooth
    // OGF_LE_CTL: Grupo de comandos LE (Low Energy)
    // OCF_LE_SET_RANDOM_ADDRESS: Comando específico para definir MAC aleatório
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_RANDOM_ADDRESS,
                     sizeof(cmd), &cmd) < 0)
    {
        perror("HCI_LE_SET_RANDOM_ADDRESS failed");
    }
}

/**
 * Configura os dados de advertising (informações transmitidas pelo dispositivo)
 * O advertising é como o dispositivo se "apresenta" para outros dispositivos próximos
 * Contém informações como nome, flags de capacidade e outros dados opcionais
 * @param sock: socket HCI do dispositivo Bluetooth
 * @param name: nome do dispositivo a ser anunciado (aparecerá nos scans)
 * @return: 0 se sucesso, -1 se erro
 */
int set_advertising_data(int sock, char *name)
{
    uint8_t data[31] = {0}; // Buffer para dados de advertising (máximo 31 bytes por especificação BLE)
    int index = 0;          // Índice para construir o pacote de dados sequencialmente

    // === CAMPO FLAGS ===
    // Flags obrigatórias que indicam as capacidades e modo de operação do dispositivo
    data[index++] = 2;    // Tamanho do campo flags (2 bytes: tipo + valor)
    data[index++] = 0x01; // Tipo de dados: Flags (AD Type)
    data[index++] = 0x06; // Flags: LE General Discoverable Mode + BR/EDR Not Supported
                          // 0x02 = LE General Discoverable (qualquer dispositivo pode descobrir)
                          // 0x04 = BR/EDR Not Supported (apenas BLE, não Bluetooth clássico)

    // === CAMPO NOME COMPLETO ===
    // Define o nome que aparecerá quando outros dispositivos escanearem
    size_t name_len = strlen(name);
    if (name_len > 29) // Limita tamanho para caber no buffer (31 - 2 bytes já usados)
        name_len = 29;
    data[index++] = name_len + 1;         // Tamanho do campo nome (nome + tipo)
    data[index++] = 0x09;                 // Tipo de dados: Complete Local Name
    memcpy(data + index, name, name_len); // Copia o nome para o buffer
    index += name_len;

    // NOTA: Código comentado - função hci_le_set_advertising_data pode não estar
    // disponível em todas as versões da biblioteca BlueZ
    // Em implementação real, seria necessário enviar estes dados para o controlador
    // if (hci_le_set_advertising_data(sock, index, data, 1000) < 0)
    // {
    //     perror("hci_le_set_advertising_data failed");
    //     return -1;
    // }
    return 0;
}

/**
 * Inicia o processo de advertising BLE
 * O advertising torna o dispositivo visível e detectável por outros dispositivos
 * Esta função configura os parâmetros de timing e comportamento do advertising
 * @param sock: socket HCI do dispositivo Bluetooth
 * @return: 0 se sucesso, -1 se erro
 */
int start_advertising(int sock)
{
    // === CONFIGURAÇÃO DOS PARÂMETROS DE ADVERTISING ===
    le_set_advertising_parameters_cp adv_params_cp;
    memset(&adv_params_cp, 0, sizeof(adv_params_cp)); // Limpa a estrutura

    // Intervalos de advertising (em unidades de 0.625ms)
    adv_params_cp.min_interval = htobs(0x00A0); // Intervalo mínimo: 160 * 0.625ms = 100ms
    adv_params_cp.max_interval = htobs(0x00A0); // Intervalo máximo: 160 * 0.625ms = 100ms
                                                // htobs: converte para byte order da rede

    adv_params_cp.advtype = 0x00;         // Tipo: ADV_IND (Connectable undirected advertising)
                                          // Permite que outros dispositivos se conectem
    adv_params_cp.own_bdaddr_type = 0x01; // Tipo de endereço: Random (usar MAC aleatório)
    adv_params_cp.chan_map = 0x07;        // Mapa de canais: 0x07 = usar todos os 3 canais (37, 38, 39)
    adv_params_cp.filter = 0x00;          // Política de filtro: sem filtros (aceitar qualquer dispositivo)

    // Envia comando HCI para configurar os parâmetros de advertising
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_PARAMETERS,
                     LE_SET_ADVERTISING_PARAMETERS_CP_SIZE,
                     &adv_params_cp) < 0)
    {
        perror("Set advertising parameters failed");
        return -1;
    }

    // === HABILITAÇÃO DO ADVERTISING ===
    le_set_advertise_enable_cp enable_cp;
    memset(&enable_cp, 0, sizeof(enable_cp)); // Limpa a estrutura
    enable_cp.enable = 0x01;                  // 0x01 = Habilita advertising, 0x00 = Desabilita

    // Envia comando HCI para iniciar o advertising
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISE_ENABLE,
                     LE_SET_ADVERTISE_ENABLE_CP_SIZE,
                     &enable_cp) < 0)
    {
        perror("Enable advertising failed");
        return -1;
    }
    return 0;
}

/**
 * Para o processo de advertising BLE
 * Esta função é importante para interromper a transmissão antes de
 * alterar configurações como MAC ou dados de advertising
 * @param sock: socket HCI do dispositivo Bluetooth
 * @return: 0 se sucesso, -1 se erro
 */
int stop_advertising(int sock)
{
    // Estrutura para comando de habilitação/desabilitação do advertising
    le_set_advertise_enable_cp enable_cp;
    memset(&enable_cp, 0, sizeof(enable_cp)); // Limpa a estrutura
    enable_cp.enable = 0x00;                  // 0x00 = Desabilita advertising

    // Envia comando HCI para parar o advertising
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISE_ENABLE,
                     LE_SET_ADVERTISE_ENABLE_CP_SIZE,
                     &enable_cp) < 0)
    {
        perror("Disable advertising failed");
        return -1;
    }
    return 0;
}

/**
 * Função principal - simula múltiplos dispositivos Bluetooth Low Energy
 * O programa alterna ciclicamente entre diferentes dispositivos simulados,
 * cada um com seu próprio MAC e nome, criando a ilusão de múltiplos
 * dispositivos físicos independentes
 */
int main()
{
    // === INICIALIZAÇÃO DO BLUETOOTH ===
    // Obtém o ID do primeiro adaptador Bluetooth disponível no sistema
    int dev_id = hci_get_route(NULL); // NULL = primeiro adaptador encontrado
    int sock = hci_open_dev(dev_id);  // Abre socket HCI para comunicação com o adaptador
    if (sock < 0)
    {
        perror("opening HCI device");
        exit(1);
    }

    // === DEFINIÇÃO DOS DISPOSITIVOS SIMULADOS ===
    // Arrays paralelos contendo os dados de cada dispositivo simulado
    char *names[] = {"DISP_01", "DISP_02", "DISP_03"};                                  // Nomes dos dispositivos
    char *macs_str[] = {"AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"}; // MACs em formato string
    bdaddr_t macs[3];                                                                   // Array para MACs convertidos para formato bdaddr_t

    // Converte strings de MAC para formato binário bdaddr_t usado pelas funções Bluetooth
    for (int i = 0; i < 3; i++)
    {
        str2ba(macs_str[i], &macs[i]); // str2ba: string to bluetooth address
    }

    int idx = 0; // Índice atual do dispositivo sendo simulado (0, 1, 2)

    // === LOOP PRINCIPAL DE SIMULAÇÃO ===
    // Loop infinito que alterna entre os dispositivos simulados
    while (1)
    {
        // Log informativo sobre qual dispositivo está sendo simulado
        printf("Broadcasting device %s with MAC %s\n", names[idx], macs_str[idx]);

        // === SEQUÊNCIA DE TROCA DE DISPOSITIVO ===
        stop_advertising(sock);          // 1. Para advertising anterior (limpa estado)
        set_random_mac(sock, macs[idx]); // 2. Define novo endereço MAC

        // 3. Configura dados de advertising com novo nome do dispositivo
        if (set_advertising_data(sock, names[idx]) < 0)
            break; // Sai do loop em caso de erro

        // 4. Inicia advertising com nova configuração (MAC + nome)
        if (start_advertising(sock) < 0)
            break; // Sai do loop em caso de erro

        // === PREPARAÇÃO PARA PRÓXIMA ITERAÇÃO ===
        idx = (idx + 1) % 3; // Alterna para próximo dispositivo (rotação circular: 0→1→2→0...)

        sleep(1); // Aguarda 1 segundo antes da próxima alternância
                  // Durante este tempo, o dispositivo atual fica visível para scanners
    }

    // === LIMPEZA E FINALIZAÇÃO ===
    // Garante que o advertising seja parado antes de finalizar o programa
    stop_advertising(sock);
    close(sock); // Fecha o socket HCI
    return 0;
}
