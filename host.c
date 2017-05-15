#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define MAXDATASIZE 512
#define SYNC 0xdcc023c2

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

//***********************************************************************//
//						ALGORITMO DO CHECKSUM							 //
//						Encontrado neste link:							 //
// https://codereview.stackexchange.com/questions/154007/16-bit-checksum-function-in-c
//***********************************************************************//

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
//***********************************************************************//
//						CHECANDO ARGUMENTOS								 //
//***********************************************************************//
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

//***********************************************************************//
//						INICIALIZANDO VARIAVEIS							 //
//***********************************************************************//

	int socketFD, newSocketFD, portNum;
	struct sockaddr_in local_addr, remote_addr;
	char *input = argv[3];
	char *output = argv[4];
	packet_t packet_send, packet_recv, pacote, ack;
	uint8_t last_id = 1;
	uint16_t last_chksum = 0;

//***********************************************************************//
//						INICIALIZANDO CONEXOES							 //
//***********************************************************************//

	socketFD = socket(AF_INET, SOCK_STREAM, 0);
	if(socketFD < 0)
	{
		fprintf(stderr, "ERROR opening socket\n");
		exit(1);
	}

	// SE CONEXÃO É PASSIVA (SERVIDOR/RECEPTOR).
	if(strcmp(argv[1], "-s") == 0)
	{
		socklen_t sockSize;

		portNum = atoi(argv[2]);
		memset((char*) &local_addr, '\0', sizeof(local_addr));
		local_addr.sin_family = AF_INET;
		local_addr.sin_port = htons(portNum);
		local_addr.sin_addr.s_addr = INADDR_ANY;

		// Inicia abertura passiva.
		if(bind(socketFD, (struct sockaddr *) &local_addr, sizeof(struct sockaddr)) < 0)
		{
			fprintf(stderr, "ERROR on binding.\n");
			exit(1);
		}

		// Fala para o socket "escutar" LISTENQUEUESIZE conexões,
		// inserindo elas em uma fila até que o accept() aceite a 
		// conexão.
		if(listen(socketFD, 1) < 0)
		{
			fprintf(stderr, "ERROR on listen port.\n");
			exit(1);
		}

		sockSize = sizeof(struct sockaddr_in);

		newSocketFD = accept(socketFD, (struct sockaddr *) &remote_addr, &sockSize);
		if(newSocketFD < 0)
		{
			fprintf(stderr, "ERROR on accept connection.\n");
			exit(1);
		}
	}
	// SE CONEXAO É ATIVA (CLIENTE/TRANSMISSOR).
	else if(strcmp(argv[1], "-c") == 0)
	{
		char *ip, *port;

		ip = strtok(argv[2], ":");
		port = strtok(NULL, " ");
		
		portNum = atoi(port);
		memset((char*) &local_addr, '\0', sizeof(struct sockaddr));
		local_addr.sin_family = AF_INET;
		local_addr.sin_port = htons(portNum);
		inet_pton(AF_INET, ip, &local_addr.sin_addr);

		//Inicia abertura ativa.
		if(connect(socketFD, (struct sockaddr *) &local_addr, sizeof(struct sockaddr)) < 0)
		{
			fprintf(stderr, "ERROR: Failed to connect. %s\n", strerror(errno));
			exit(1);
		}
	}
//***********************************************************************//
//***********************************************************************//
//							ENVIANDO DADOS								 //
//***********************************************************************//
//***********************************************************************//
	FILE *fsend = fopen(input, "r");
	if(fsend == NULL)
	{
		fprintf(stderr, "ERROR: %s file cannot be opened", input);
		fclose(fsend);
		exit(1);
	}

	int blockSize;
	uint8_t buffer[MAXDATASIZE];

	while((blockSize = fread(buffer, sizeof(uint8_t), MAXDATASIZE, fsend)) >= 0)
	{	printf("1\n");
		memset(&packet_send, '\0', sizeof(packet_t));
		packet_send.sync1 = htonl(SYNC);
		packet_send.sync2 = htonl(SYNC);
		packet_send.chksum = htons(0);
		packet_send.length = htons(blockSize);
		last_id == 1 ? (packet_send.id = htons(0)) : (packet_send.id = htons(1));
		blockSize == MAXDATASIZE ? (packet_send.flags = htons(0)) : (packet_send.flags = htons(64)); //64 = b'0100 0000
		memcpy(packet_send.data, &buffer, blockSize);
		printf("2\n");
		packet_send.chksum = htons((uint16_t) csum((unsigned short *) &packet_send, sizeof(packet_t)));
		printf("3\n");
		printf("%s\n", buffer);
		if(send(socketFD, (packet_t *) &packet_send, sizeof(packet_t), 0) < 0)
		{
			fprintf(stderr, "ERROR on sending file. Aborted.\n");
			exit(1);
		}
		printf("4\n");
		struct timeval timeout;
		timeout.tv_sec = 1; // segundos
		timeout.tv_usec = 0; // microsegundos
		printf("5\n");
		setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &timeout, sizeof(struct timeval));
		if(recv(socketFD, (packet_t *) &packet_recv, sizeof(packet_t), 0) < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				fseek(fsend, -blockSize, SEEK_CUR);
				continue;
			}
		}
		printf("6\n");

		packet_recv.id = ntohs(packet_recv.id);
		if(packet_recv.id != (ntohs(packet_send.id))); //if -> while
		{
			fseek(fsend, -blockSize, SEEK_CUR);
			continue;
		}
		printf("7\n");

		last_id == 1 ? (last_id = 0) : (last_id = 1);
	}
	printf("File sent to server");
	fclose(fsend);
//***********************************************************************//
//***********************************************************************//
//							RECEBENDO DADOS								 //
//***********************************************************************//
//***********************************************************************//
	FILE *frecv = fopen(output, "w");
	if(frecv == NULL)
	{
		fprintf(stderr, "ERROR: %s file cannot be opened. Aborted.", output);
		fclose(frecv);
		exit(1);
	}
	printf("8\n");
	while(1)
	{
		bool flag_end = false;
		memset(&pacote, '\0', sizeof(packet_t));
		if(recv(newSocketFD, (packet_t *) &pacote, sizeof(packet_t), 0) < 0)
		{
			// Erro de recebimento se negativo. errno é setado.
			break;
		}

		unsigned short checksum = csum((unsigned short *)&pacote, sizeof(packet_t));

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
		if(checksum != 0)
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
		else if(last_id == pacote.id)
		{
			printf("ID igual ao do ultimo pacote recebido.\n");
			continue;
		}

		if(pacote.flags == 64) // 0100 0000
		{
			// Pacote com bit END ligado
			flag_end = true;
		}

		last_id = pacote.id;
		last_chksum = pacote.chksum;

		int writeSize = fwrite(pacote.data, sizeof(uint8_t), pacote.length, frecv);
		if(writeSize < pacote.length)
		{
			fprintf(stderr, "ERROR on write file\n");
		}
		if(flag_end)
			break;

		// TEM QUE USAR HTONS AQUI!!!
		memset(&ack, '\0', sizeof(packet_t));
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
	fclose(frecv);
	return 0;
}
