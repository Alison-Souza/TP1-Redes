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

	int socketFD, newSocketFD, useSocketFD, portNum, flagEND = false;
	struct sockaddr_in local_addr, remote_addr;
	char *input = argv[3];
	char *output = argv[4];
	packet_t packet_send, packet_recv, ack;
	uint8_t last_id = 1, last_id_recv = 1;
	uint16_t last_chksum_recv = 0;

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

		if(listen(socketFD, 1) < 0)
		{
			fprintf(stderr, "ERROR on listen port.\n");
			exit(1);
		}

		socklen_t sockSize = sizeof(struct sockaddr_in);

		newSocketFD = accept(socketFD, (struct sockaddr *) &remote_addr, &sockSize);
		if(newSocketFD < 0)
		{
			fprintf(stderr, "ERROR on accept connection.\n");
			exit(1);
		}

		useSocketFD = newSocketFD;
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

		useSocketFD = socketFD;
	}
//***********************************************************************//
//***********************************************************************//
//							TRANSMITINDO DADOS							 //
//***********************************************************************//
//***********************************************************************//
	FILE *fsend = fopen(input, "r");
	if(fsend == NULL)
	{
		fprintf(stderr, "ERROR: %s file cannot be opened. Aborted.\n", input);
		fclose(fsend);
		exit(1);
	}

	FILE *frecv = fopen(output, "w");
	if(frecv == NULL)
	{
		fprintf(stderr, "ERROR: %s file cannot be opened. Aborted.\n", output);
		fclose(frecv);
		exit(1);
	}

	int blockSize;
	uint8_t buffer[MAXDATASIZE];

	// Enquanto há blocos pra enviar, ou não recebeu último pacote
	while((blockSize = fread(buffer, sizeof(uint8_t), MAXDATASIZE, fsend)) > 0 || flagEND != true)
	{
		// TRANSMISSOR COMEÇA AQUI
		if(blockSize >= 0)
		{
			memset(&packet_send, '\0', sizeof(packet_t));
			packet_send.sync1 = htonl(SYNC);
			packet_send.sync2 = htonl(SYNC);
			packet_send.chksum = htons(0);
			packet_send.length = htons(blockSize);
			last_id == 1 ? (packet_send.id = 0) : (packet_send.id = 1);//(packet_send.id = htons(0)) : (packet_send.id = htons(1));
			blockSize == MAXDATASIZE ? (packet_send.flags = 0) : (packet_send.flags = 64);//(packet_send.flags = htons(0)) : (packet_send.flags = htons(64)); //END -> 64 = b'0100 0000
			memcpy(packet_send.data, &buffer, blockSize);
			packet_send.chksum = htons((uint16_t) csum((unsigned short *) &packet_send, sizeof(packet_t)));
			if(send(useSocketFD, (packet_t *) &packet_send, sizeof(packet_t), 0) < 0)
			{
				fprintf(stderr, "ERROR on sending file. Aborted.\n");
				exit(1);
			}
		}	
		// RECEPTOR COMEÇA AQUI.
		// Tudo que eu recebo após o outro lado parar de transmitir serão
		// acks. Mas tenho que entrar nesse loop pra receber os acks.
		while(true) // Enquanto não chega o ack e tem coisa pra receber.
		{
			struct timeval timeout;
			timeout.tv_sec = 1; // segundos
			timeout.tv_usec = 0; // microsegundos
			setsockopt(useSocketFD, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &timeout, sizeof(struct timeval));
			if(recv(useSocketFD, (packet_t *) &packet_recv, sizeof(packet_t), 0) < 0)
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
				{
					// PRECISA FAZER ISSO? RECV PODE GERAR QUALQUER PACOTE,
					// NÃO APENAS ACKS.
					fseek(fsend, -blockSize, SEEK_CUR);
					break;
				}
			}
			// Faz isso porque checksum é transformado em NBO após calculo.
			packet_recv.chksum = ntohs(packet_recv.chksum);
			unsigned short checksum = csum((unsigned short *)&packet_recv, sizeof(packet_t));
			if(checksum != 0)
			{
				fprintf(stderr, "Checksum received is %d . Invalid value to checksum. Aborted.\n", checksum);
				exit(1);
			}

			packet_recv.sync1 = ntohl(packet_recv.sync1);
			packet_recv.sync2 = ntohl(packet_recv.sync2);
			packet_recv.chksum = ntohs(packet_recv.chksum);
			packet_recv.length = ntohs(packet_recv.length);
			//packet_recv.id = ntohs(packet_recv.id);
			//packet_recv.flags = ntohs(packet_recv.flags);

			if(packet_recv.sync1 != SYNC || packet_recv.sync2 != SYNC)
			{
				fprintf(stderr, "ERROR: packet received is not syncronized\n");
				exit(1);
			}
			if(packet_recv.flags == 128 && packet_recv.length == 0) // É ack?
			{
				if(packet_recv.id == (ntohs(packet_send.id))) // É ack esperado?
				{
					// envia o próximo pacote, parando esse loop

					// LAST_ID ALTERA AQUI???
					last_id == 1 ? (last_id = 0) : (last_id = 1);
					break; //last_id deve ser alterado.
				}
				else //Se não é o ack esperado
				{
					// Reenvia último pacote
					fseek(fsend, -blockSize, SEEK_CUR);
					break; //last_id NÃO deve ser alterado
				}
			}
			else // Se não é ack.
			{
				if(packet_recv.id == last_id_recv && packet_recv.chksum == last_chksum_recv) // É o mesmo pacote anterior?
				{
					// reenvia último ack
					if(send(useSocketFD, (packet_t*) &ack, sizeof(packet_t), 0) < 0)
					{
						fprintf(stderr, "ERROR on resending last ack. Errno: %s\n", strerror(errno));
						exit(1);
					}
				}
				else // Se é pacote novo, escreve no arquivo e envia ack.
				{
					// Escreve no arquivo.
					int writeSize = fwrite(packet_recv.data, sizeof(uint8_t), packet_recv.length, frecv);
					if(writeSize < packet_recv.length)
					{
						fprintf(stderr, "ERROR on write in file %s\n", output);
						exit(1);
					}


					if(packet_recv.flags == 64) // bit END, último pacote recebido.
					{
						flagEND = true;
					}
			
					// Prepara e envia ACK.
					memset(&ack, '\0', sizeof(packet_t));
					ack.sync1 = htonl(SYNC);
					ack.sync2 = htonl(SYNC);
					ack.chksum = htons(0);
					ack.length = htons(0);
					ack.id = packet_recv.id;//htons(packet_recv.id);
					ack.flags = 128;//htons(128); // ACK flag - 1000 0000
					memset(ack.data, '\0', MAXDATASIZE);

					ack.chksum = htons((uint16_t) csum((unsigned short*) &ack, sizeof(packet_t)));
					if(send(useSocketFD, (packet_t*) &ack, sizeof(packet_t), 0) < 0)
					{
						fprintf(stderr, "ERROR on sending ack. Errno: %s\n", strerror(errno));
						exit(1);
					}
					last_id_recv = packet_recv.id;
					last_chksum_recv = packet_recv.chksum;
				}
			}
		}
	}
	fclose(fsend);
	fclose(frecv);
	return 0;
}
