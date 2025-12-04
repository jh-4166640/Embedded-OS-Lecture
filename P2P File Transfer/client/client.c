#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <pthread.h>
#include <dirent.h>
#include <sys/wait.h>


#define SERV_IP     "220.149.128.92"
#define SERV_PORT   4485 // 고정
#define MYP2P_IP    "220.149.128.9x" // 할 때 마다 바꿔줘야 함
#define MYP2P_PORT  448x // 클라이언트 포트 // 할 때 마다 바꿔줘야 함

#define BACKLOG      10
#define ACCEPT_ERROR -1
#define SOCKET_ERROR -2

#define DATA_NOT_RECEIVED -1


/* Log in 관련 숫자 */
#define LOG_IN_SUCCESS_VAL		1
#define LOG_IN_FAIL_VAL		 	2
#define NOT_FIND_USER_VAL		3

#define USER_036

#ifdef  USER_025
#define P2P_SHARE_DIR_PATH "/home/st2021146025/P2P_shared_files"
#else
#define P2P_SHARE_DIR_PATH "/home/st2021146036/P2P_shared_files"
#endif



typedef struct{
    int sockfd;
    char rx[512];
} thread_data_t;

int Socket_Init(int *sockfd, struct sockaddr_in *addr, short port, const char *ip);
void Send_Message(int sockfd, char *msg);
int Recv_Message(int sockfd, char *buf);
int Server_Log_in(int sockfd, char *name);
void Group_Chatting(int sockfd, char *name);
void* Recv_Message_Process(void *arg);

int P2P_Server_Init(int *sockfd, struct sockaddr_in *my_addr);
void P2P_Server();
void P2P_Client(short port, const char *ip);
void List_Shared_Files(const char *path, char *file_list, size_t list_size);



int Socket_Init(int *sockfd, struct sockaddr_in *addr, short port, const char *ip)
{
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd == -1)
    {
        perror("Client-socket() error-lol!");
        return ACCEPT_ERROR;
    }
    else printf("Client-socket() sockfd is OK...\n");
    
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = inet_addr(ip);
    
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
    
    int accept_status=Socket_Init(&sockfd, &dest_addr, SERV_PORT, SERV_IP);
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
    Send_Message(sockfd, MYP2P_IP);
    char port_buf[16];
    sprintf(port_buf, "%d", MYP2P_PORT);
    Send_Message(sockfd,port_buf);
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
            if(strncmp(buf,"$FILE",5)==0) // P2P file request command
            {
                print("$FILE command received.\n");
                int pid=fork();
                if(pid<0)
                {
                    perror("fork() error lol!");
                    continue;
                }
                else if(pid == 0) // child process
                {
                    buf[strcspn(buf, "\n")] = '\0';
                    char *token = strtok(buf, "|");  // $FILE
                    token = strtok(NULL, "|");         // userN
                    char *ip = strtok(NULL, "|");      // ip
                    char *port = strtok(NULL, "|");    // port
                    if(token != NULL && ip != NULL && port != NULL)
                    {
                        short p2p_port = (short)atoi(port);
                        P2P_Client(p2p_port, ip); // File 전송하는 쪽이 Client
                    }
                    exit(0);
                }
                else if(pid > 0) // parent process
                {
                    waitpid(pid, NULL, WNOHANG);  // Prevent zombie processes
                }
                
            }
            else printf("%s\n",buf);
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
        else if(strncmp(tx_buf,"$FILE",5)==0) // P2P file request command
        // full command $FILE <USER_N>
        {
            int pid = fork();
            if(pid<0)
            {
                perror("fork() error lol!");
                continue;
            }
            else if(pid == 0) // child process
            {
                P2P_Server(); // File 받는 쪽이 Server
                
            }
            else if(pid > 0) // parent process
            {
                waitpid(pid, NULL, WNOHANG);  // Prevent zombie processes
            }
        }
        else Send_Message(sockfd, tx_buf);
    }
}

void List_Shared_Files(const char *path, char *file_list, size_t list_size)
{
    DIR *dp;
    struct dirent *entry;
    dp = opendir(path);
    if(dp == NULL)
    {
        perror("opendir error");
        return;
    }
    printf("Shared files in %s:\n", path);
    file_list[0] = '\0'; // Initialize file_list

    while ((entry = readdir(dp)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;
        if(strlen(file_list) + strlen(entry->d_name) + 2 >= list_size)
            break;
        strcat(file_list, entry->d_name);
        strcat(file_list, "|");
        printf("%s\n", entry->d_name);
    }
    closedir(dp);
}

int P2P_Server_Init(int *sockfd, struct sockaddr_in *my_addr)
{
    int val = 1;
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd == -1)
    {
        perror("P2P Server-socket() error-lol!");
        return SOCKET_ERROR;
    }
    else printf("P2P Server-socket() sockfd is OK...\n");
    my_addr->sin_family = AF_INET;
    my_addr->sin_port = htons(MYP2P_PORT); // Client가 P2P 에서 Server가 되는 Port
    my_addr->sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr->sin_zero),0,8);
    if(setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR,(char*)&val,sizeof(val))<0)
	{
		perror("setsockiot");
		close(*sockfd);
		return SOCKET_ERROR;
	}
    if(bind(*sockfd,(struct sockaddr *)my_addr, sizeof(struct sockaddr))== -1)
    {
        perror("P2P Server-bind() error lol!");
        exit(1);
        return SOCKET_ERROR;
    }
    else printf("P2P Server-bind() is OK...\n");

    if(listen(*sockfd, BACKLOG)== -1)
    {
        perror("P2P listen() error lol!");
        exit(1);
        return SOCKET_ERROR;
    }
    else printf("P2P listen() is OK...\n\n");
    return 0;
}


void P2P_Server() // File을 받는 쪽
{
    int P2Pserver_fd, P2Pclient_fd;
	struct sockaddr_in my_addr;
	struct sockaddr_in client_addr;
	unsigned int sin_size;

	/* buffer */
	int rcv_byte;
	char buf[512];
    /* P2P Server 소켓 초기화 */
	int sock_res = P2P_Server_Init(&P2Pserver_fd, &my_addr);
    if(sock_res == SOCKET_ERROR)
    {
        printf("P2P Server initialization failed.\n");
        return;
    }
    else if(sock_res == 0)
    {
        sin_size = sizeof(struct sockaddr_in);
        P2Pclient_fd = accept(P2Pserver_fd, (struct sockaddr *)&client_addr, &sin_size);   
        if(P2Pclient_fd < 0)
        {
            perror("accept() error lol!");
            return;
        }
        // P2P connection established, now can send/receive files
        Send_Message(P2Pclient_fd, "P2P Conneected");
    }
    Recv_Message(P2Pclient_fd, buf);
    for(int i=0; i<strlen(buf); i++)
    {
        if(buf[i] == '|')
            buf[i] = '\n';
    }
    printf("============file list============\n%s\n", buf);
    close(P2Pclient_fd);
}

void P2P_Client(short port, const char *ip) // File을 전송하는 쪽
{
    int P2Pserver_fd;
    struct sockaddr_in server_addr;
    char buf[512];
    char file_list[512];

    int sock_res = Socket_Init(&P2Pserver_fd, &server_addr, port, ip);
    if(sock_res == ACCEPT_ERROR)
    {
        exit(1);
        return;
    }
    Recv_Message(P2Pserver_fd, buf);
    printf("%s\n",buf);
    List_Shared_Files(P2P_SHARE_DIR_PATH, file_list, sizeof(file_list));
    Send_Message(P2Pserver_fd, file_list);

    printf("P2P Client is closing ... \n");
    close(P2Pserver_fd);
    return;

}

