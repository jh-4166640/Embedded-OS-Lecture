#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <pthread.h>


#define SERV_IP     "220.149.128.92"
#define SERV_PORT   4480 // 고정
//#define P2P_PORT    4001

#define DATA_NOT_RECEIVED -1

/* Log in 관련 숫자 */
#define LOG_IN_SUCCESS_VAL		0
#define LOG_IN_FAIL_VAL		 	1
#define NOT_FIND_USER_VAL		2


void Send_Message_Process(int sockfd, char *msg);
int Recv_Message_Process(int sockfd, char *buf);
int Server_Log_in(int sockfd);
void Group_Chatting_Process(int sockfd, char *msg, char *buf);


int main(void)
{
    int sockfd;
    struct sockaddr_in dest_addr;
    
    int rcv_byte;
    char rx_buf[512];
    char tx_buf[512];
    char id[20];
    char pw[20];

    pthread_t recv_tid;
    pthread_t file_wait_tid;

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
    
    Server_Log_in(sockfd);
    close(sockfd); 
    return 0;
}

void Send_Message_Process(int sockfd, char *msg)
{
    int msg_len = strlen(msg);
    send(sockfd, &msg_len,sizeof(msg_len),0);
    send(sockfd, msg, strlen(msg), 0);
}

int Recv_Message_Process(int sockfd, char *buf)
{
    int rcv_byte, msg_len = 0, len = 0;

    rcv_byte=recv(sockfd,&msg_len,sizeof(msg_len),0); // received data length
    if(rcv_byte>0)
    {
        while(len < msg_len)
        {
            int rcv = recv(sockfd, buf + len, msg_len-len,0);
            if(rcv <= 0) return -1; //Data Not received
            len += rcv;
        }
        buf[len] = '\0';
        //printf("rx data : %s\n",buf);
        return 0;
    }
    return DATA_NOT_RECEIVED; // Data Not received
}

int Server_Log_in(int sockfd)
{
    int rcv_byte;
    char buf[512];
    char id[20];
    char pw[20];
    char send_msg[512];
    int div_idx = 0;
    char msg[128];
    char res[2];


    rcv_byte = recv(sockfd, buf, sizeof(buf), 0);
    printf("%s\n",buf);
   
    printf("ID: ");
    fgets(id,sizeof(id),stdin);
    id[strcspn(id,"\n")] = '\0';
    
    printf("PW: ");
    fgets(pw,sizeof(pw),stdin);
    pw[strcspn(pw,"\n")] = '\0';
    
    snprintf(send_msg, sizeof(send_msg),"%s|%s",id,pw);
    Send_Message_Process(sockfd, send_msg);
    //printf("tx data(size,data): (%d, %s)\n",msg_len,send_msg);
    rcv_byte = recv(sockfd, buf, sizeof(buf), 0); // Login 결과 수신

    char *pdiv = strchr(buf,'|');
    if(pdiv != NULL)
    {
        div_idx = pdiv-buf;
        strncpy(msg, buf, div_idx);
        msg[div_idx]='\0';
        int res_len = strlen(buf) - div_idx - 1;
        strncpy(res, pdiv+1,res_len);

        res[res_len]='\0';
    }
    else return DATA_NOT_RECEIVED;

    printf("%s\n\n",msg);
    return atoi(res);
}