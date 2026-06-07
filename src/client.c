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

#define PORT 4433
#define BUFFER_SIZE 2048

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

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s <IP_SERVIDOR>\n", argv[0]);
        exit(1);
    }

    int tun_fd, sock_fd;
    struct sockaddr_in server_addr;
    char tun_name[IFNAMSIZ] = "tun0";
    unsigned char buffer[BUFFER_SIZE];
    struct pollfd fds[2];

    // crear e inicializiar el el tun cliente
    tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) { perror("Error TUN"); exit(1); }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip addr add 10.0.0.2 peer 10.0.0.1 dev %s && ip link set dev %s up", tun_name, tun_name);
    if (system(cmd) != 0)
    {
        fprintf(stderr, "Error al configurar la interfaz %s\n", tun_name);
        close(tun_fd);
        exit(1);
    }
    printf("[+] Cliente TUN %s configurado (10.0.0.2)\n", tun_name);

    // conectarse al servidor
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Error socket"); close(tun_fd); exit(1); }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "IP invalida: %s\n", argv[1]);
        close(tun_fd);
        close(sock_fd);
        exit(1);
    }

printf("[+] conectando al servidor %s:%d...\n", argv[1], PORT);

if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
{
perror("Error de conexión");
exit(1);
}
printf("[+] Conectado al tunel con exito. \n");

// configurando la poll
fds[0].fd = tun_fd;
fds[0].events = POLLIN;
fds[1].fd = sock_fd;
fds[1].events = POLLIN;

// bucle espejo de la VPN
while(poll(fds, 2, -1) > 0)
{   
    if(fds[0].revents & POLLIN)
    {
    ssize_t nread = read(tun_fd, buffer, sizeof(buffer));
    if(nread > 0){ send(sock_fd, buffer, nread, 0);}
    }
    if(fds[1].revents & POLLIN)
    {
    ssize_t nread = recv(sock_fd, buffer, sizeof(buffer),0);
    if(nread <= 0) {printf("[-] Conexion interrumpida por el servidor\n"); break; }
    write(tun_fd, buffer, nread);
    }
}

close(sock_fd); close(tun_fd);
return 0;
}