#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define MAX_DATA_SIZE 512
#define MAX_PACKET_SIZE 1024

char buffer[MAX_DATA_SIZE];
int next_seq_num = 0;

// estrutura de um pacote RDT
struct RDT_Packet { 
    int seq_num;  // número de sequência
    int ack;      // flag de acuso
    char data[MAX_DATA_SIZE];  // dados
    int checksum; // soma de verificação
    int size_packet;// tamanho do pacote
};

void send_ack(int socketfd, struct sockaddr_in *server_address, int ack_number){
  char send_buffer[MAX_DATA_SIZE] = "";
  sprintf(send_buffer, "%d", ack_number);
  int check = sendto(socketfd, (char *) send_buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) server_address, sizeof(*server_address));
  if(check == -1){
    perror("Falha ao enviar o ack");
  }
}

void rdt_recv(int socketfd, struct sockaddr_in *server_address, int num_pckt) {
    struct RDT_Packet packet;
    int i = 0;
    int timeoutMax = 0; //variavel de controle para o maximo de timeouts tolerados pelo servidor
    int tolen = sizeof(server_address);
    while(i < num_pckt && timeoutMax < 3){ //tolerancia de aguardo
      int messageReceived = recvfrom(socketfd, &packet, sizeof(packet), MSG_WAITALL, ( struct sockaddr *) server_address, &tolen);;
      memcpy(buffer, packet.data, sizeof(buffer)); //desserialização do pacote
      printf("O contéudo do pacote é: %s\n", packet.data);
      int seq_number = packet.seq_num; 
      if(messageReceived == -1){ // pacote foi recebido?
        printf("Erro ao receber pacote");
            break;
        }else{
          printf("Pacote recebido com sucesso\n");
          if(seq_number == next_seq_num){ // sequencia de n° do pacote está correta?
          send_ack(socketfd, server_address, packet.ack); // envia ack
          timeoutMax = 0; // zera o contador
          i+=1; //receba o próximo pacote
            if (seq_number == 1){
                next_seq_num = 0;
            }
            else{
              next_seq_num = 1;
            }
          
          } else {
          printf("Erro: número de sequência errado\n");
            send_ack(socketfd, server_address, next_seq_num);// envia ack
            
            //int resend_message = recvfrom(socketfd, &packet, sizeof(packet), MSG_WAITALL, ( struct sockaddr *)  server_address, &tolen);
            timeoutMax+=1; //caso seq_num esteja errado, aumente o contador
        }
      }
      //timeoutMax+=1; //aguarde o reenvio do pacote
    }
  }

void rdt_send(int socketfd, struct sockaddr_in *server_address, char *request, int request_number){
    
    int check = sendto(socketfd, (const char *)request, strlen(request), MSG_CONFIRM, (const struct sockaddr *) server_address, sizeof(*server_address));
    if(check != -1){
      printf("Enviando requisição...\n");
      rdt_recv(socketfd, server_address, request_number);
    } else {
      printf("Erro: Falha ao enviar requisição ao servidor");  
    }
}

int main(int argc, char *argv[]) {
    int socketfd;
    struct sockaddr_in server_address;
    char request[MAX_DATA_SIZE] = "";
    
      if (argc != 4)
    {
        printf("Usage: ./server <ip> <port> <numéro de pacotes> \n");
        return -1;
    }
  
    // Criando o socket
    socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Verificando se o socket foi criado
    if (socketfd < 0) {
        perror("Erro na criacao do socket");
        exit(1);
    }
  
    //memset(&server_address, 0, sizeof(server_address));
    // Configurando o endereco
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[2])); //argv[2]
    server_address.sin_addr.s_addr = inet_addr(argv[1]);

   int num_pckt = atoi(argv[3]);
   sprintf(request, "%d", num_pckt);
    // Conectando ao servidor
    /*(if (connect(socketfd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("Erro na conexao");
        exit(1);
    }*/

    printf("Conectado com sucesso\n");
    rdt_send(socketfd, &server_address, request, num_pckt);
    close(socketfd);

    return 0;
}