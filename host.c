#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAXDATASIZE 512
#define SYNC 0xdcc023c2
#define LISTENQUEUESIZE 5

typedef enum {false, true} bool;

typedef struct packet
{
	uint32_t sync1;
	uint32_t sync2;
	uint16_t chksum;
	uint16_t length;
	uint8_t id;
	uint8_t flags;
	uint8_t data[MAXDATASIZE];
} packet_t;

unsigned short csum(unsigned short *ptr, int nbytes) 
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while(nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    if(nbytes == 1)
    {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;

    return(answer);
}

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "ERROR: Flags and parameters not set.\n");
		exit(1);
	}
	else if(argc < 5)
	{
		if(strcmp(argv[1], "-s") == 0)
		{
			fprintf(stderr, "ERROR: Args should be -s <PORT> <INPUT> <OUTPUT>\n");
			exit(1);
		}
		else if(strcmp(argv[1], "-c") == 0)
		{
			fprintf(stderr, "ERROR: Args should be -c <IPPAS>:<PORT> <INPUT> <OUTPUT>\n");
			exit(1);
		}
		else
		{
			fprintf(stderr, "ERROR: Flags should be -c or -s.\n");
			exit(1);
		}
	}

	int socketFD, newSocketFD, portNum;
	struct sockaddr_in serv_addr, client_addr;
	char *input = argv[3];
	char *output = argv[4];
	socklen_t sockSize;
	packet_t pacote;
	uint8_t last_id = 1;
	uint16_t last_chksum = 0;

	socketFD = socket(AF_INET, SOCK_STREAM, 0);
	if(socketFD < 0)
	{
		fprintf(stderr, "ERROR opening socket\n");
		exit(1);
	}

	if(strcmp(argv[1], "-s") == 0) // Se é servidor.
	{
		portNum = atoi(argv[2]);
		bzero((char*) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(portNum);
		serv_addr.sin_addr.s_addr = INADDR_ANY;

		// Inicia abertura passiva.
		if(bind(socketFD, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		{
			fprintf(stderr, "ERROR on binding\n");
			exit(1);
		}

		// Fala para o socket "escutar" LISTENQUEUESIZE conexões,
		// inserindo elas em uma fila até que o accept() aceite a 
		// conexão.
		listen(socketFD, LISTENQUEUESIZE);

		sockSize = sizeof(struct sockaddr_in);

		while(1)
		{
			newSocketFD = accept(socketFD, (struct sockaddr *) &client_addr, &sockSize);
			if(newSocketFD < 0)
			{
				fprintf(stderr, "ERROR on accept connection\n");
				exit(1);
			}

			FILE *fp = fopen(output, "w");
			if(fp == NULL)
			{
				fprintf(stderr, "ERROR: output file cannot be opened");
				fclose(fp);
				exit(1);
			}
			else
			{
				int fp_blockSize = 0;
				bzero(&pacote, sizeof(packet_t));
				while(1)
				{
					bool flag_end = false;
					fp_blockSize = recv(newSocketFD, (packet_t *) &pacote, sizeof(packet_t), 0);
					if(fp_blockSize < 0)
					{
						break;
					}

					unsigned short chksm_packet = ntohs(pacote.chksum);
					pacote.chksum = 0;
					unsigned short cs = csum((unsigned short *)&pacote, sizeof(packet_t));

					pacote.sync1 = ntohl(pacote.sync1);
					pacote.sync2 = ntohl(pacote.sync2);
					pacote.chksum = ntohs(pacote.chksum);
					pacote.length = ntohs(pacote.length);
					pacote.id = ntohs(pacote.id);
					pacote.flags = ntohs(pacote.flags);
					if(pacote.length > 512 || pacote.length < 0)
					{
						fprintf(stderr, "pacote.length esta errado\n");
						break;
					}
					if(cs != chksm_packet)
					{
						fprintf(stderr, "Checksum não confere, dado corronpido\n");
						continue;
					}
					if(last_id == pacote.id && last_chksum == pacote.chksum)
					{
						printf("Retrasmissão detectada. Reinviando confirmação\n");
						continue;
						//Reenvia último ACK
					}
					if(pacote.flags == 64)
					{
						// Pacote com bit END ligado
						flag_end = true;
					}

					last_id = pacote.id;
					last_chksum = pacote.chksum;

					int writeSize = fwrite(pacote.data, sizeof(uint8_t), pacote.length, fp);
					if(writeSize < pacote.length)
					{
						fprintf(stderr, "ERROR on write file\n");
					}
					if(flag_end)
						break;

					packet_t ack;
					bzero(&ack, sizeof(packet_t));
					ack.sync1 = SYNC;
					ack.sync2 = SYNC;
					ack.chksum = 0;
					ack.length = 0;
					ack.id = pacote.id;
					ack.flags = 128; // 1000 0000
					memset(ack.data, '\0', MAXDATASIZE);

					ack.chksum = htons((uint16_t) csum((unsigned short*) &ack, sizeof(packet_t)));
					
					if(send(newSocketFD, (packet_t*) &ack, sizeof(packet_t), 0) < 0);
					{
						fprintf(stderr, "ERROR on sending ack. ID: %d\n", ack.id);
					}
				}
				printf("Received from client\n");
				fclose(fp);
			}
		}
	}
	else if(strcmp(argv[1], "-c") == 0) // Se é cliente.
	{
	}

	return 0;
}
