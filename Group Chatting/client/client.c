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

#define ACCEPT_ERROR -1

#define DATA_NOT_RECEIVED -1

/* Log in 관련 숫자 */
#define LOG_IN_SUCCESS_VAL		1
#define LOG_IN_FAIL_VAL		 	2
#define NOT_FIND_USER_VAL		3

typedef struct{
    int sockfd;
    char rx[512];
} thread_data_t;

int Socket_Init(int *sockfd, struct sockaddr_in *addr);
void Send_Message(int sockfd, char *msg);
int Recv_Message(int sockfd, char *buf);
int Server_Log_in(int sockfd, char *name);
void Group_Chatting(int sockfd, char *name);
void* Recv_Message_Process(void *arg);


int Socket_Init(int *sockfd, struct sockaddr_in *addr)
{
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd == -1)
    {
        perror("Client-socket() error-lol!");
        return ACCEPT_ERROR;
    }
    else printf("Client-socket() sockfd is OK...\n");
    
    addr->sin_family = AF_INET;
    addr->sin_port = htons(SERV_PORT);
    addr->sin_addr.s_addr = inet_addr(SERV_IP);
    
    memset(&(addr->sin_zero),0,8);
    
    if(connect(*sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr))==-1)
    {
        perror("Client-connect() error lol");
        return ACCEPT_ERROR;
    }   
    else printf("Client-connect() is OK...\n\n");

    return 0;
}


int main(void)
{
    int sockfd;
    struct sockaddr_in dest_addr;
    char name[20];
    
    int accept_status=Socket_Init(&sockfd, &dest_addr);
    if(accept_status == ACCEPT_ERROR)
    {
        exit(1);
    }
    
    int login_status = Server_Log_in(sockfd, name);
    if(login_status == LOG_IN_SUCCESS_VAL)
    {
        printf("=================================\n");
        Group_Chatting(sockfd, name);
    }
    
    printf("Client is closing ... \n");
    close(sockfd); 
    return 0;
}


void Send_Message(int sockfd, char *msg)
{
    int msg_len = strlen(msg);
    send(sockfd, &msg_len,sizeof(msg_len),0);
    send(sockfd, msg, strlen(msg), 0);
}

int Recv_Message(int sockfd, char *buf)
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

int Server_Log_in(int sockfd, char *name)
{
    int rcv_byte;
    char buf[128];
    char id[20];
    char pw[20];
    char send_msg[128];
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
    Send_Message(sockfd, send_msg);
    //printf("tx data(size,data): (%d, %s)\n",msg_len,send_msg);
    rcv_byte = recv(sockfd, buf, sizeof(buf), 0); // Login 결과 수신

    char *pdiv = strchr(buf,'|'); // Log in 결과의 구분자 찾기
    if(pdiv != NULL)
    {
        /* Log in 결과 문장 메세지 */
        div_idx = pdiv-buf;
        strncpy(msg, buf, div_idx);
        msg[div_idx]='\0';

        /* Log in 결과 예약어 메세지 */
        int res_len = strlen(buf) - div_idx - 1;
        strncpy(res, pdiv+1,res_len);
        strncpy(name, id, sizeof(id));
        res[res_len]='\0';
    }
    else return DATA_NOT_RECEIVED;

    printf("%s\n",msg);
    return atoi(res);
}


void* Recv_Message_Process(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    int sockfd = data->sockfd;
    char *buf = data->rx;

    while(1)
    {
        int rcv_status = Recv_Message(sockfd, buf);
        // Wait for messages from server
        if(rcv_status == 0) // Message received successfully
        {
            printf("%s\n",buf);
        }
        else if(rcv_status == DATA_NOT_RECEIVED)
        {
            printf("Data not received or connection closed by server.\n");
            free(data);
            break;
        }
    }
    return NULL;
}
void Group_Chatting(int sockfd, char *name)
{
    pthread_t recv_tid;
    //pthread_t file_wait_tid;
    char tx_buf[512];

    thread_data_t *thread_data = malloc(sizeof(thread_data_t));
    thread_data->sockfd = sockfd;
    memset(thread_data->rx, 0, sizeof(thread_data->rx));

    pthread_create(&recv_tid, NULL, Recv_Message_Process, thread_data); // Recieve thread
    pthread_detach(recv_tid);
    while(1) // Transmit loop
    {
        fgets(tx_buf,sizeof(tx_buf),stdin); // Wait for user input
        tx_buf[strcspn(tx_buf,"\n")] = '\0';
        if(strcmp(tx_buf,"exit")==0)
        {
            close(sockfd);
            break;
        }
        else Send_Message(sockfd, tx_buf);
    }
}