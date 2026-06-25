#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    if((fd = open("/dev/net/tun", O_RDWR)) < 0) return fd;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if(*dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    if((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) { close(fd); return err; }
    strncpy(dev, ifr.ifr_name, IFNAMSIZ);
    return fd;
}

int main(int argc, char *argv[])
{
    if(argc < 2){ printf("Uso: %s <IP_SERVIDOR>\n", argv[0]); exit(1);}
    
    int tun_fd, sock_fd;
    struct sockaddr_in server_addr;
    char tun_name[IFNAMSIZ] = "tun1";
    
    unsigned char buffer[BUFFER_SIZE];
    unsigned char enc_buffer[CRYPTO_BUFFER_SIZE];
    struct pollfd fds[2];
    
    tun_fd = tun_alloc(tun_name);
    if(tun_fd < 0){ perror("Error TUN"); exit(1);}

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip addr add 10.0.0.2 peer 10.0.0.1 dev %s && ip link set dev %s up", tun_name, tun_name);
    if(system(cmd) != 0){ perror("Configuracion de interfaz fallida"); exit(1);}
    printf("[+] Cliente TUN %s configurado (10.0.0.2)\n", tun_name);

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){ perror("Error socket"); exit(1);}
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) { fprintf(stderr, "IP Invalida %s\n", argv[1]); exit(1);}

    if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {perror("Error de conexion"); exit(1);}
    
    // Enviar PING de inicialización activa
    send(sock_fd, "PING", 4, 0);
    printf("[+] Conectado (UDP) al servidor %s y enviado PING de control\n", argv[1]);

    fds[0].fd = tun_fd;
    fds[0].events = POLLIN;
    fds[1].fd = sock_fd;
    fds[1].events = POLLIN;
    
    while(poll(fds, 2, -1) > 0)
    {
        if(fds[0].revents & POLLIN)
        {
            ssize_t nread = read(tun_fd, buffer, sizeof(buffer));
            if(nread > 0){
                int enc_len = vpn_encrypt(buffer, nread, enc_buffer);
                if (enc_len > 0) {
                    send(sock_fd, enc_buffer, enc_len, 0);
                }
            }   
        } 
         
        if(fds[1].revents & POLLIN)
        {
            ssize_t nread = recv(sock_fd, enc_buffer, sizeof(enc_buffer), 0);
            if(nread > 0){
                // Filtro de seguridad: Evitar procesar ecos de PING residuales en modo cifrado
                if (nread == 4 && memcmp(enc_buffer, "PING", 4) == 0) {
                    continue;
                }
                
                int dec_len = vpn_decrypt(enc_buffer, nread, buffer);
                if (dec_len > 0) {
                    write(tun_fd, buffer, dec_len);
                }
            }
        }
    }
    close(sock_fd); close(tun_fd);
    return 0;
}