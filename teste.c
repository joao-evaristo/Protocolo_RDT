#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_BUF_SIZE 1024
#define MAX_DATA_SIZE 512
#define ADLER_BASE 65521 

char buffer[MAX_BUF_SIZE];
struct sockaddr_in client_address;
int next_ack = 1;
int next_seq_num = 0;



// estrutura de um pacote RDT
struct RDT_Packet
{
    int seq_num;              // número de sequência
    int ack;                  // flag de acuso
    char data[MAX_DATA_SIZE]; // dados
    unsigned int checksum;             // soma de verificação
    int size_packet;
} Packet;

unsigned int adler32(unsigned char *data, size_t len) {
  unsigned int a = 1, b = 0;

  while (len > 0) {
    size_t tlen = len > 5550 ? 5550 : len;
    len -= tlen;
    do {
      a += *data++;
      b += a;
    } while (--tlen);

    a %= ADLER_BASE;
    b %= ADLER_BASE;
  }

  return (b << 16) | a;
}

// cria um pacote com base nos dados de entrada
struct RDT_Packet create_packet(int seq_num, int ack, char *data, int size_packet, unsigned int checksum)
{
    struct RDT_Packet packet;
    // área da memória do pacote é zerada
    memset(&packet, 0, sizeof(packet));

    // parâmetros são passados
    packet.seq_num = seq_num;
    packet.ack = ack;
    packet.size_packet = size_packet;

    // o payload é copiado para o pacote
    memcpy(packet.data, data, size_packet);

    // soma de verificação
    packet.checksum = checksum;

    return packet;
}

void generate_payload(char *message, int request_number)
{
    int i;
    for (i = 0; i < request_number; i++)
    {
        int random_payload = (rand() % request_number);
        sprintf(message, "%d", random_payload);
        memcpy(buffer, message, sizeof(message));
    }
}


void parameters(struct RDT_Packet packet){
    if (packet.seq_num == 0){
       next_seq_num = 1;
    } else {
       next_seq_num = 0;
    }
    if (packet.ack == 0)
    {
       next_ack = packet.ack+2;
    } else {
       next_ack = 1;
    }
}

// envia um pacote para um socket
void rdt_send(int socketfd, int request_number)
{
    struct timeval timeout;
    timeout.tv_sec = 5;  // segundos
    timeout.tv_usec = 0; // microssegundos

    // timeout para recebimento
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        perror("setsockopt(..., SO_RCVTIMEO ,...");

    int timeoutMax = 0; // variavel de controle para o maximo de timeouts tolerados pelo servidor
    int i = 0;
    int resend_counter = 0;
    int tolen = sizeof(client_address);
while (i < request_number && timeoutMax < 3){ 
	// Caso não receba o ack 3 vezes consecutivas

	char message[MAX_BUF_SIZE] = "";
	generate_payload(message, request_number);
	unsigned int aux_checksum = adler32(buffer, strlen((char *)buffer));
	struct RDT_Packet packet = create_packet(next_seq_num, next_ack - 1, buffer, strlen(buffer), aux_checksum); // cria o pacote
	int messageSent = sendto(socketfd, &packet, sizeof(struct RDT_Packet), MSG_CONFIRM, (struct sockaddr *)&client_address, tolen);

	printf("Pacote %d, timeouts: %d\n", packet.seq_num, timeoutMax);
	printf("O checksum do pacote é: %d\n", packet.checksum);

	int resend = messageSent;

  bool resend_counter = false;

	while(resend == -1 && timeoutMax < 3){
		resend_counter = true;
		printf("Erro: o ack está errado. Reenviando último pacote...\n");
		resend = sendto(socketfd, &packet, sizeof(struct RDT_Packet), MSG_CONFIRM, (struct sockaddr *)&client_address, tolen);

		if(resend == -1)
			timeoutMax++;
	}
	
	if(timeoutMax < 3)
		timeoutMax = 0;
	else{
		printf("TimeOut: Ack errado\n");
		break;
	}
  
	memset(buffer, 0, sizeof(buffer));

	int ack_receive = recvfrom(socketfd, (char *)buffer, MAX_BUF_SIZE, MSG_WAITALL, (struct sockaddr *)&client_address, &tolen);
	
	int timeoutMaxAux = 0;
	
	while(ack_receive == -1 && timeoutMax < 3){ 
		timeoutMax++;
    resend_counter = true;
		
		while(resend == -1 && timeoutMaxAux < 3){
			printf("Erro: o ack está errado. Reenviando último pacote...\n");
			resend = sendto(socketfd, &packet, sizeof(struct RDT_Packet), MSG_CONFIRM, (struct sockaddr *)&client_address, tolen);

			if(resend == -1)
				timeoutMaxAux++;
		}
		
		if(timeoutMaxAux < 3)
			ack_receive = recvfrom(socketfd, (char *)buffer, MAX_BUF_SIZE, MSG_WAITALL, (struct sockaddr *)&client_address, &tolen);
		else
			timeoutMax = 3;
	}
	
	if(timeoutMax < 3){
		packet.ack = atoi(buffer);
		timeoutMax = 0;
	}
	else{
		printf("TimeOut: Ack não recebido\n");
		break;
	}
	
	printf("Ack: %d\n", packet.ack);
	
	if (packet.seq_num == 0)
		next_seq_num = 1;
	else
		next_seq_num = 0;
	
	if (packet.ack == 0)
		next_ack = packet.ack + 2;
	else
		next_ack = 1;
	
	if(!resend_counter)
		i++;
    }
  }



void rdt_recv(int socketfd)
{
    int tolen, n;

    tolen = sizeof(client_address);
    int request = recvfrom(socketfd, (char *)buffer, MAX_BUF_SIZE, MSG_WAITALL, (struct sockaddr *)&client_address, &tolen);
    int request_number = atoi(buffer);
    if (request != -1)
    {
        printf("Enviando pacotes...\n");
        rdt_send(socketfd, request_number);
    }
    else
    {
        printf("Erro: falha ao enviar o pacote\n");
    }
}

int main(int argc, char *argv[])
{
    int socket_fd, bytes;

    socklen_t server_length;
    unsigned int sequence_number = 0;
    int ack_number;

    // Verifica se o usuário passou os argumentos corretamente
    if (argc != 2)
    {
        printf("Uso: ./server <porta>\n");
        return -1;
    }

    // Abre o socket UDP
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("Erro ao inicializar o socket\n");
        return -1;
    }

    // Configura o endereço do servidor
    // memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(atoi(argv[1]));
    // inet_aton(argv[1], client_address.sin_addr.s_addr);
    client_address.sin_addr.s_addr = INADDR_ANY;

    // Aguarda a conexão
    if (bind(socket_fd, (struct sockaddr *)&client_address, sizeof(client_address)) < 0)
    {
        perror("bind()");
        printf("Erro ao realizar o bind\n");
        return -1;
    }
    printf("Aguardando conexão...\n");

    rdt_recv(socket_fd);
    return 0;
}
