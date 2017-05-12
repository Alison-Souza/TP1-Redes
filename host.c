#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>

#define MAXDATASIZE 512
#define SYNC 0xdcc023c2

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
			fprintf(stderr, "ERROR: Args should be <PORT> <INPUT> <OUTPUT>\n");
			exit(1);
		}
		else if(strcmp(argv[1], "-c") == 0)
		{
			fprintf(stderr, "ERROR: Args should be <IPPAS>:<PORT> <INPUT> <OUTPUT>\n");
			exit(1);
		}
		else
		{
			fprintf(stderr, "ERROR: Flags should be -c or -s.\n");
			exit(1);
		}
	}

	int socketFD;

	socketFD = socket(AF_INET, SOCK_STREAM, 0);
	if(socketFD < 0)
	{
		fprintf(stderr, "ERROR opening socket\n");
		exit(1);
	}


	return 0;
}

/* CHECKSUM DA INTERNET
** LENGTH E ID EM NETWORK BYTE ORDER
** FLAGS:	bit 7	 --> ACK
**			bit 6	 --> END
** 			bits 5-0 --> devem ser zero
** TRANSMISSOR:
**     Lê dados de arquivo e transmite.
**     Cria os pacotes, transmite o pacote, retransmite (a cada 1 seg) 
**     até ser confirmado. Troca o ID, e transmite próximo quadro.
** RECEPTOR:
**     Lê dados do soquete e escreve em arquivo.
**     Aceita o pacote apenas se ID_atual != ID_anterior, e se não houver 
**     erros. Cria e envia o ACK, com ID igual ao do pacote a confirmar. 
**     Verifica se o pacote recebido tem o mesmo ID e checksum do pacote
**     anterior (retransmissão). Se tiver, reenvia ACK.
** DETECÇÃO ERROS:
**     Usando checksum.
**     Verificando duas ocorrências do SYNC.
** FIM DE TRANSMISSÂO:
**     Transmissor 
**         Ligar bit END no último pacote. (ACK nunca tem END ligado)
**     Receptor
**         Confirma o quadro com bit END ligado normalmente.
**         Salva a informação que não há mais pacotes por receber.
**         Se não houver o que transmitir, encerra conexão e execução.
** 
** 
** DIAGRAMA DO PACOTE
** 
** 0 	32 		64 		80 			96 	104 	112 			112+length
** +---/----+---/----+---------+---------+-----+-----+------ ... ---+
** | SYNC | SYNC | chksum | length | ID |flags| DADOS 				|
** +---/----+---/----+---------+---------+-----+-----+------ ... ---+
** dcc023c2 dcc023c2 faef    0004    00    00    01020304
** 
*/