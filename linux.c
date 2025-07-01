#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <string.h>

/**
 * Define um endereço MAC aleatório para o dispositivo Bluetooth
 * @param sock: socket HCI do dispositivo
 * @param mac: endereço MAC a ser configurado
 */
void set_random_mac(int sock, bdaddr_t mac)
{
    struct
    {
        uint8_t bdaddr[6]; // Array de 6 bytes para o endereço MAC
    } __attribute__((packed)) cmd;

    // Copia o MAC para a estrutura de comando
    memcpy(cmd.bdaddr, &mac, 6);

    // Envia comando HCI para definir endereço aleatório
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_RANDOM_ADDRESS,
                     sizeof(cmd), &cmd) < 0)
    {
        perror("HCI_LE_SET_RANDOM_ADDRESS failed");
    }
}

/**
 * Configura os dados de advertising (informações transmitidas)
 * @param sock: socket HCI do dispositivo
 * @param name: nome do dispositivo a ser anunciado
 * @return: 0 se sucesso, -1 se erro
 */
int set_advertising_data(int sock, char *name)
{
    uint8_t data[31] = {0}; // Buffer para dados de advertising (máximo 31 bytes)
    int index = 0;

    // Flags - indica capacidades do dispositivo
    data[index++] = 2;    // Tamanho do campo flags
    data[index++] = 0x01; // Tipo: Flags
    data[index++] = 0x06; // Flags: LE General Discoverable + BR/EDR Not Supported

    // Nome completo do dispositivo
    size_t name_len = strlen(name);
    if (name_len > 29) // Limita tamanho para caber no buffer
        name_len = 29;
    data[index++] = name_len + 1;         // Tamanho do campo nome
    data[index++] = 0x09;                 // Tipo: Complete Local Name
    memcpy(data + index, name, name_len); // Copia o nome
    index += name_len;

    // Código comentado - enviaria os dados para o dispositivo
    // if (hci_le_set_advertising_data(sock, index, data, 1000) < 0)
    // {
    //     perror("hci_le_set_advertising_data failed");
    //     return -1;
    // }
    return 0;
}

/**
 * Inicia o processo de advertising BLE
 * @param sock: socket HCI do dispositivo
 * @return: 0 se sucesso, -1 se erro
 */
int start_advertising(int sock)
{
    // Configura parâmetros de advertising
    le_set_advertising_parameters_cp adv_params_cp;
    memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.min_interval = htobs(0x00A0); // Intervalo mínimo: 100ms
    adv_params_cp.max_interval = htobs(0x00A0); // Intervalo máximo: 100ms
    adv_params_cp.advtype = 0x00;               // Tipo: Connectable undirected advertising
    adv_params_cp.own_bdaddr_type = 0x01;       // Usar endereço aleatório
    adv_params_cp.chan_map = 0x07;              // Usar todos os 3 canais de advertising
    adv_params_cp.filter = 0x00;                // Sem filtros

    // Envia comando para configurar parâmetros
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_PARAMETERS,
                     LE_SET_ADVERTISING_PARAMETERS_CP_SIZE,
                     &adv_params_cp) < 0)
    {
        perror("Set advertising parameters failed");
        return -1;
    }

    // Habilita o advertising
    le_set_advertise_enable_cp enable_cp;
    memset(&enable_cp, 0, sizeof(enable_cp));
    enable_cp.enable = 0x01; // Habilita advertising

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
 * @param sock: socket HCI do dispositivo
 * @return: 0 se sucesso, -1 se erro
 */
int stop_advertising(int sock)
{
    le_set_advertise_enable_cp enable_cp;
    memset(&enable_cp, 0, sizeof(enable_cp));
    enable_cp.enable = 0x00; // Desabilita advertising

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
 * Função principal - simula múltiplos dispositivos Bluetooth
 * alternando entre diferentes MACs e nomes
 */
int main()
{
    // Obtém o ID do primeiro adaptador Bluetooth disponível
    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id); // Abre socket HCI
    if (sock < 0)
    {
        perror("opening HCI device");
        exit(1);
    }

    // Arrays com nomes e MACs dos dispositivos simulados
    char *names[] = {"DISP_01", "DISP_02", "DISP_03"};
    char *macs_str[] = {"AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"};
    bdaddr_t macs[3]; // Array para MACs convertidos

    // Converte strings MAC para formato bdaddr_t
    for (int i = 0; i < 3; i++)
    {
        str2ba(macs_str[i], &macs[i]);
    }

    int idx = 0; // Índice atual do dispositivo sendo simulado

    // Loop infinito alternando entre os dispositivos
    while (1)
    {
        printf("Broadcasting device %s with MAC %s\n", names[idx], macs_str[idx]);

        stop_advertising(sock);          // Para advertising anterior
        set_random_mac(sock, macs[idx]); // Define novo MAC

        // Configura dados de advertising com novo nome
        if (set_advertising_data(sock, names[idx]) < 0)
            break;

        // Inicia advertising com nova configuração
        if (start_advertising(sock) < 0)
            break;

        idx = (idx + 1) % 3; // Alterna para próximo dispositivo (circular)

        sleep(1); // Aguarda 1 segundo antes da próxima alternância
    }

    // Cleanup: para advertising e fecha socket
    stop_advertising(sock);
    close(sock);
    return 0;
}
