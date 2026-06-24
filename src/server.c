#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <poll.h>
#include "../lib/crypto.h"

#define PORT 4433
#define BUFFER_SIZE 2048

#define CRYPTO_BUFFER_SIZE (BUFFER_SIZE + IV_LEN + TAG_LEN)

int tun_alloc(char *dev)
{
    struct ifreq ifr;
    int fd, err;
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        return fd;

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (*dev)
        strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
    {
        if (errno == EBUSY && *dev)
        {
            memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
            if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) >= 0)
            {
                strncpy(dev, ifr.ifr_name, IFNAMSIZ);
                return fd;
            }
        }
        close(fd);
        return err;
    }

    strncpy(dev, ifr.ifr_name, IFNAMSIZ);
    return fd;
}

int main(void)
{
    int tun_fd, server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char tun_name[IFNAMSIZ] = "tun0";
    unsigned char buffer[BUFFER_SIZE];
    unsigned char enc_buffer[CRYPTO_BUFFER_SIZE];

    struct pollfd fds[2];
    int client_connected = 0; // Al ser UDP, sabremos del cliente al recibir el primer paquete

    tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0)
    {
        perror("Error TUN");
        exit(1);
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip addr add 10.0.0.1 peer 10.0.0.2 dev %s && ip link set dev %s up", tun_name, tun_name);
    if (system(cmd) != 0)
    {
        perror("Configuración interfaz fallida");
        exit(1);
    }
    printf("[+] Servidor TUN %s configurado (10.0.0.1)\n", tun_name);

    // Creamos socket UDP (SOCK_DGRAM)
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0)
    {
        perror("Error socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error bind");
        exit(1);
    }
    printf("[+] Servidor UDP escuchando en el puerto %d...\n", PORT);

    fds[0].fd = tun_fd;
    fds[0].events = POLLIN;
    fds[1].fd = server_fd;
    fds[1].events = POLLIN;

    while (poll(fds, 2, -1) > 0)
    {
        // Datos saliendo desde el túnel hacia el cliente remoto
        // TUN->SOCKET (Trafico saliendo al cliente cifrado)
        if (fds[0].revents & POLLIN)
        {
            ssize_t nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread > 0 && client_connected)
            {
                int enc_len = vpn_encrypt(buffer, nread, enc_buffer);
                if (enc_len > 0)
                {
                    sendto(server_fd, enc_buffer, enc_len, 0, (struct sockaddr *)&client_addr, client_len);
                }
            }
        }

        // Datos entrantes desde el cliente remoto hacia el túnel
        // SOCKET -> TUN (trafico entrando del cliente desencriptado)
        if (fds[1].revents & POLLIN)
        {
            ssize_t nread = recvfrom(server_fd, enc_buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
            if (nread > 0)
            {
                if (!client_connected)
                {
                    printf("[+] Cliente remoto detectado desde %s:%d\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    client_connected = 1;
                    if (nread == 4 && memcmp(buffer, "PING", 4) == 0)
                    {
                        continue;
                    }
                }
                int dec_len = vpn_decrypt(enc_buffer, nread, buffer);
                if (dec_len < 0)
                {
                    write(tun_fd, buffer, nread);
                }
                else
                {
                    printf("[-] Error de descifrado o paquete manipulado ignorado \n");
                }
            }
        }
    }

    close(server_fd);
    close(tun_fd);
    return 0;
}
