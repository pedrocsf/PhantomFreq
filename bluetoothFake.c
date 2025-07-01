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
 * @brief Define um endereço MAC aleatório estático para o adaptador.
 * * @param sock O socket HCI.
 * @param mac O endereço (bdaddr_t) a ser definido.
 */
void set_random_mac(int sock, bdaddr_t mac)
{
    struct
    {
        uint8_t bdaddr[6];
    } __attribute__((packed)) cmd;

    memcpy(cmd.bdaddr, &mac, 6);

    // Envia o comando HCI para definir o endereço MAC aleatório
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_RANDOM_ADDRESS,
                     sizeof(cmd), &cmd) < 0)
    {
        perror("HCI_LE_SET_RANDOM_ADDRESS failed");
    }
}

/**
 * @brief Define os dados que serão enviados no pacote de anúncio (advertising).
 * * @param sock O socket HCI.
 * @param name O nome do dispositivo a ser anunciado.
 * @return int 0 em caso de sucesso, -1 em caso de falha.
 */
int set_advertising_data(int sock, char *name)
{
    uint8_t data[31] = {0};
    int index = 0;

    // Campo 1: Flags
    // LE General Discoverable Mode, BR/EDR Not Supported.
    data[index++] = 2;    // Comprimento do campo
    data[index++] = 0x01; // Tipo: Flags
    data[index++] = 0x06;

    // Campo 2: Service UUID
    // Adiciona o UUID do Serviço de Frequência Cardíaca (0x180D) para simulação realista.
    data[index++] = 3;    // Comprimento do campo
    data[index++] = 0x03; // Tipo: Complete List of 16-bit Service Class UUIDs
    data[index++] = 0x0D; // UUID (0x180D em little-endian)
    data[index++] = 0x18;

    // Campo 3: Nome Local Completo
    size_t name_len = strlen(name);
    if (name_len > (31 - index - 2)) // Garante que não exceda o tamanho máximo
        name_len = (31 - index - 2);

    data[index++] = name_len + 1; // Comprimento do campo
    data[index++] = 0x09;         // Tipo: Complete Local Name
    memcpy(data + index, name, name_len);
    index += name_len;

    // Envia o comando HCI para definir os dados do anúncio
    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_DATA, index, data) < 0)
    {
        perror("HCI_LE_SET_ADVERTISING_DATA failed");
        return -1;
    }

    return 0;
}

/**
 * @brief Configura os parâmetros e inicia o anúncio BLE.
 * * @param sock O socket HCI.
 * @return int 0 em caso de sucesso, -1 em caso de falha.
 */
int start_advertising(int sock)
{
    le_set_advertising_parameters_cp adv_params_cp;
    memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.min_interval = htobs(0x00A0); // 100ms
    adv_params_cp.max_interval = htobs(0x00A0); // 100ms
    adv_params_cp.advtype = 0x00;               // ADV_IND (Conectável e escaneável)
    adv_params_cp.own_bdaddr_type = 0x01;       // Usa o endereço aleatório configurado
    adv_params_cp.chan_map = 0x07;              // Usa todos os 3 canais de anúncio
    adv_params_cp.filter = 0x00;

    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_PARAMETERS,
                     LE_SET_ADVERTISING_PARAMETERS_CP_SIZE,
                     &adv_params_cp) < 0)
    {
        perror("Set advertising parameters failed");
        return -1;
    }

    le_set_advertise_enable_cp enable_cp;
    memset(&enable_cp, 0, sizeof(enable_cp));
    enable_cp.enable = 0x01; // Habilitar anúncio

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
 * @brief Para o anúncio BLE.
 * * @param sock O socket HCI.
 * @return int 0 em caso de sucesso, -1 em caso de falha.
 */
int stop_advertising(int sock)
{
    le_set_advertise_enable_cp enable_cp;
    memset(&enable_cp, 0, sizeof(enable_cp));
    enable_cp.enable = 0x00; // Desabilitar anúncio

    if (hci_send_cmd(sock, OGF_LE_CTL, OCF_LE_SET_ADVERTISE_ENABLE,
                     LE_SET_ADVERTISE_ENABLE_CP_SIZE,
                     &enable_cp) < 0)
    {
        perror("Disable advertising failed");
        return -1;
    }
    return 0;
}

int main()
{
    int dev_id = hci_get_route(NULL);
    if (dev_id < 0)
    {
        perror("Nenhum dispositivo HCI disponível");
        exit(1);
    }

    // Derruba a interface para garantir um estado limpo antes de começar.
    // Requer privilégios de root (execute com sudo).
    // if (hci_dev_down(dev_id) < 0)
    // {
    //     perror("Falha ao desativar o dispositivo HCI (pode já estar down)");
    // }

    // Ativa a interface novamente para uso exclusivo.
    // if (hci_dev_up(dev_id) < 0)
    // {
    //     perror("Falha ao ativar o dispositivo HCI");
    //     exit(1);
    // }

    int sock = hci_open_dev(dev_id);
    if (sock < 0)
    {
        perror("Falha ao abrir o dispositivo HCI");
        exit(1);
    }

    // Arrays de MACs e nomes para alternar
    char *names[] = {"HRM_Brac_01", "HRM_Brac_02", "HRM_Brac_03"};
    char *macs_str[] = {"AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"};
    bdaddr_t macs[3];
    for (int i = 0; i < 3; i++)
    {
        str2ba(macs_str[i], &macs[i]);
    }

    printf("Iniciando simulação de dispositivos BLE...\n");

    int idx = 0;
    while (1)
    {
        printf("Transmitindo como: %s | MAC: %s\n", names[idx], macs_str[idx]);

        // A ordem é importante: parar -> configurar -> iniciar
        stop_advertising(sock);
        set_random_mac(sock, macs[idx]);

        if (set_advertising_data(sock, names[idx]) < 0)
            break;

        if (start_advertising(sock) < 0)
            break;

        // Pausa para que o dispositivo possa ser detectado por scanners
        sleep(2);

        // Alterna para o próximo dispositivo
        idx = (idx + 1) % 3;
    }

    // Limpeza final ao sair do loop (ex: com Ctrl+C)
    printf("\nParando anúncio e fechando socket.\n");
    stop_advertising(sock);
    close(sock);
    return 0;
}