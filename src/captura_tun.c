#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_tun.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048

int main(void)
{
int tun_fd;
unsigned char buffer[BUFFER_SIZE];
ssize_t nread;

tun_fd = open("/dev/tun0", O_RDWR);

	if(tun_fd < 0)
	{
	perror("Error to open /dev/tun0 ¿do you forget to use doas?"); exit(1);
	}

printf("[+] Interface /dev/tun0 open sucessfully\n");
printf("[+] Configure tun0 with ifconfig...\n");
int status = system("ifconfig tun0 10.0.0.1 10.0.0.2 up");

	if(status !=0)
	{
	fprintf(stderr, "Error to configure interface tun0\n");
	close(tun_fd);
	exit(1);
	}

printf("[+] Listening packages IP/ICMP in tun0. Press Ctrl+C to exit\n\n");



while(1)
	{
	nread = read(tun_fd, buffer, sizeof(buffer));
	
	if(nread < 0)
	{
	perror("Error to read tun0");
	break;
	}
	

	printf("--------------------------------------------\n");
	printf("¡package catched! size:%ld bytes\n");
	unsigned char version = (buffer[0] >> 4)& 0x0F;
	unsigned char protocol = buffer[9];
	printf("Version IP: IPv%d\n", version);
	printf("Protocolo (Raw ID): %d", protocol);

	if(protocolo == 1)
	{
	printf("(ICMP/Ping)\n");
	unsigned char tipo_icmp = buffer[20];
		
		if(tipo_icmp == 8)
		{
		printf("[message] ICMP Echo Request (ping request)\n");
		}
			else if(tipo_icmp == 0)
			{
			printf("[message] ICMP Echo Reply (answer ping)\n");
			}	
	}
	else
	{
	printf("(Another protocol)\n");
	}


	printf("Dump (Hex): ");

		for(ssize_t i = 0; i < (nread > 24 ? 24:nread); i++)
		{
		printf("%02x",buffer[i]);
		}
	printf(nread > 24 ? "... \n" :"\n");
}


close(tun_fd);
return 0;
}





