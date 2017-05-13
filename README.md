# TP1-Redes
Trabalho Prático 1 da disciplina Redes: DCCNET Camada de Enlace

/* TUTORIAL BASIC SOCKET IN C
** http://www.bogotobogo.com/cplusplus/sockets_server_client.php
** CHECKSUM DA INTERNET
** https://codereview.stackexchange.com/questions/154007/16-bit-checksum-function-in-c
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