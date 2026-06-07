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

#define PORT 4433
#define BUFFER_SIZE 2048

int tun_alloc(char *dev)
{
struct ifreq ifr;
int fd, err;

if ((fd = open("/dev/net/tun", O_RDWR)) < 0) return fd;

memset(&ifr, 0, sizeof(ifr));
ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

if (*dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

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
int tun_fd, server_fd, client_fd;
struct sockaddr_in server_addr;
char tun_name[IFNAMSIZ] = "tun0";
unsigned char buffer[BUFFER_SIZE];
struct pollfd fds[2];

// crear y abrir el tunel
tun_fd = tun_alloc(tun_name);
if (tun_fd < 0) { perror("Error TUN"); exit(1); }
char cmd[256];
snprintf(cmd, sizeof(cmd), "ip addr add 10.0.0.1 peer 10.0.0.2 dev %s && ip link set dev %s up", tun_name, tun_name);
if (system(cmd) != 0) { fprintf(stderr, "Error al configurar la interfaz %s\n", tun_name); close(tun_fd); exit(1); }
printf("[+] Servidor TUN %s configurado (10.0.0.1)\n", tun_name);

// crear y abrir el socket TCP
server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd < 0) { perror("Error socket"); close(tun_fd); exit(1); }
int opt = 1;
if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { perror("setsockopt"); close(server_fd); close(tun_fd); exit(1); }
memset(&server_addr, 0, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = INADDR_ANY;
server_addr.sin_port = htons(PORT);

if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { perror("bind"); close(server_fd); close(tun_fd); exit(1); }
if (listen(server_fd, 1) < 0) { perror("listen"); close(server_fd); close(tun_fd); exit(1); }
printf("[+] Servidor escuchando conexiones en el puerto %d....\n", PORT);
client_fd = accept(server_fd, NULL, NULL);
if (client_fd < 0) { perror("accept"); close(server_fd); close(tun_fd); exit(1); }
printf("[+] Cliente remoto conectado a la VPN. \n");
    

// conectar multiplexacion con poll
fds[0].fd = tun_fd;
fds[0].events = POLLIN;
fds[1].fd = client_fd;
fds[1].events = POLLIN;

// el bucle bidireccional de la vpn
while(poll(fds, 2 , -1) > 0)
{

    if (fds[0].revents & POLLIN) {
    ssize_t nread = read(tun_fd, buffer, sizeof(buffer));
    if (nread > 0) {
        printf("[TUN -> SOCK] paquete %zd bytes\n", nread);
        send(client_fd, buffer, nread, 0);
    }
}
if (fds[1].revents & POLLIN) {
    ssize_t nread = recv(client_fd, buffer, sizeof(buffer), 0);
    if (nread <= 0) { perror("Error recv o cliente desconectado"); break; }
    printf("[SOCK -> TUN] paquete %zd bytes\n", nread);
    write(tun_fd, buffer, nread);
}


}

close(client_fd); close(server_fd); close(tun_fd);
return 0;
}

