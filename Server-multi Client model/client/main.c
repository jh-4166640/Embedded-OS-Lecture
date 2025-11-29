#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERV_IP     "220.149.128.92"
#define SERV_PORT   4000
#define P2P_PORT    4001

int main(void)
{
    int sockfd;
    struct sockaddr_in dest_addr;
    
    int rcv_byte;
    char buf[512];
    char id[20];
    char pw[20];
    char send_msg[512];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        perror("Client-socket() error-lol!");
        exit(1);
    }
    else printf("Client-socket() sockfd is OK...\n");
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERV_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(SERV_IP);
    
    memset(&(dest_addr.sin_zero),0,8);
    
    if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))==-1)
    {
        perror("Client-connect() error lol");
        exit(1);
    }   
    else printf("Client-connect() is OK...\n\n");
    
    rcv_byte = recv(sockfd, buf, sizeof(buf), 0);
    printf("%s\n",buf);
   
    printf("ID: ");
    fgets(id,sizeof(id),stdin);
    id[strcspn(id,"\n")] = '\0';

    printf("PW: ");
    fgets(pw,sizeof(pw),stdin);
    pw[strcspn(pw,"\n")] = '\0';
    
    snprintf(send_msg, sizeof(send_msg),"%s|%s",id,pw);
    send(sockfd, send_msg, strlen(send_msg), 0);

    rcv_byte = recv(sockfd, buf, sizeof(buf), 0);
    printf("%s\n",buf);

    close(sockfd); 
    return 0;
}