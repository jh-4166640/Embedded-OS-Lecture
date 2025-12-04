#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include <pthread.h>

#define SERV_IP		"220.149.128.92"
#define SERV_PORT	4480 // 고정
#define BACKLOG		10
/* Client_Log_in return code */
#define MALFUNCTION 		-2
#define RECV_EERROR 		-1
#define RECV_NO_ERROR 		0
#define DATAFRAMES_MISMATCH -3

#define INIT_MSG "=========================\nHello! I'm P2P File Sharing Server\nPlease, LOG-IN!\n=========================\n"
#define LOG_IN_SUCCESS_VAL		1
#define LOG_IN_FAIL_VAL		 	2
#define NOT_FIND_USER_VAL		3

#define LOG_IN_SUCCESS(msg,user) sprintf(msg,"Log-in success [%s]",user)
#define LOG_IN_SUCCESS_STR(msg, user) sprintf(msg,"Welcome to the TUlk [%s]",user)
#define LOG_IN_FAIL				 "Log-in fail: Incorrect password..."
#define NOT_FIND_USER			 "Not Find user ID"
#define MAX_USER 2

char *user_ID[MAX_USER]= {"user1","user2"};
char *user_PW[MAX_USER]= {"passwd1","passwd2"};

#define DATA_NOT_RECEIVED -1

typedef struct{
    int sockfd;
    char rx[512];
} thread_data_t;


void Send_Message(int sockfd, char *msg);
int Recv_Message(int sockfd, char *buf);
void Group_Chatting(int sockfd);
void* Recv_Message_Process(void *arg);

int Client_Log_in(int client_fd, char *buf);
int Find_user(char *target);



int main(void)
{
	int sockfd, new_fd;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	unsigned int sin_size;
	
	/* buffer */
	int rcv_byte;
	char buf[512];
	char msg[512];
	int val = 1;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0); // socket(~,~,protocol); protocol == 0 mean Auto Set protocol
	if(sockfd == -1)
	{
		printf("Server-socket() erorr lol!");
	}
	else printf("Server-socket() sockfd is OK...\n");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(SERV_PORT); // Big Endian sort short type args
	my_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(my_addr.sin_zero),0,8);
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(char*)&val,sizeof(val))<0)
	{
		perror("setsockiot");
		close(sockfd);
		return -1;
	}

	if(bind(sockfd,(struct sockaddr *)&my_addr, sizeof(struct sockaddr))== -1)
	{
		perror("Server-bind() error lol!");
		exit(1);
	}
	else printf("Server-bind() is OK...\n");
	
	if(listen(sockfd, BACKLOG)== -1)
	{
		perror("listen() error lol!");
		exit(1);
	}
	else printf("listen() is OK...\n\n");

	while(1)
	{
		sin_size = sizeof(struct sockaddr_in);
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);	
		if(new_fd < 0)
		{
			perror("accept() error lol!");
			continue;
		}
		
		int pid=fork();
		if(pid<0)
		{
			perror("fork() error");
			close(new_fd);
			continue;
		}
		else if(pid == 0) // child process
		{
			send(new_fd , INIT_MSG, strlen(INIT_MSG)+1,0);
			int receive_res = Client_Log_in(new_fd,buf);
			switch(receive_res)
			{
				case LOG_IN_SUCCESS_VAL:
					/* Client와 Group_Talk */
					Group_Chatting(new_fd);
					break;
				case LOG_IN_FAIL_VAL:
					break;
				case NOT_FIND_USER_VAL:
					break;
				case RECV_NO_ERROR:
					break;
				case RECV_EERROR:
					printf("Data received Error\n\n");
					break;
				case DATAFRAMES_MISMATCH:
					printf("Dataframes mismatch\n\n");
					break;
				case MALFUNCTION:
				default:
					printf("Client_Log_in() Malfunction\n\n");
			}
			
			memset(buf, 0, sizeof(buf));
			close(new_fd);
		}
		else if(pid > 0) // parent process
		{
			close(new_fd);		
		}
	}
	
	close(new_fd);
	close(sockfd);

	return 0; 
}

int Client_Log_in(int client_fd, char *buf)
{
	int rcv_byte, msg_len = 0, len = 0;
	char id[20];
	char pw[20];
	char msg[512];
	char *pdiv;
	int div_idx = 0, user_idx = -1;

	rcv_byte=recv(client_fd,&msg_len,sizeof(msg_len),0); // received data length
	if(rcv_byte>0)
	{
		while(len < msg_len)
		{
			int rcv = recv(client_fd, buf + len, msg_len-len,0);
			if(rcv <= 0) return RECV_EERROR; //Data Not received
			len += rcv;
		}
		buf[len] = '\0';
		pdiv = strchr(buf,'|');
		if(pdiv != NULL)
		{
			//printf("rx data : %s\n",buf);
			div_idx = pdiv-buf;
			strncpy(id, buf,div_idx);
			id[div_idx]='\0';
			int pw_len = strlen(buf) - div_idx - 1;
			strncpy(pw, pdiv+1,pw_len);

			pw[pw_len]='\0';
			user_idx = Find_user(id);
			
			printf("=========================\nUser Information\n");
			printf("ID: %s, PW: %s\n",id,pw);
			printf("=========================\n");
			
			if(user_idx >= 0)
			{
				if(strcmp(user_PW[user_idx],pw) == 0) // Log in success
				{
					char temp[64];
					char send_temp[64];
					LOG_IN_SUCCESS(temp,id);
					printf("%s\n\n",temp);

					LOG_IN_SUCCESS_STR(send_temp, id);
					sprintf(msg,"%s|%d",send_temp,LOG_IN_SUCCESS_VAL);
					send(client_fd , msg, strlen(msg)+1,0);
					//printf("%s\n\n",msg);
					return LOG_IN_SUCCESS_VAL;
				}
				else // Log in fail
				{
					sprintf(msg,"%s|%d",LOG_IN_FAIL,LOG_IN_FAIL_VAL);
					send(client_fd , msg, strlen(msg)+1,0);
					printf("%s\n\n",LOG_IN_FAIL);
					return LOG_IN_FAIL_VAL;
				}
			}
			else // Not found user ID
			{
				sprintf(msg,"%s|%d",NOT_FIND_USER,NOT_FIND_USER_VAL);
				send(client_fd , msg, strlen(msg)+1,0);
				printf("%s\n\n",NOT_FIND_USER);
				return NOT_FIND_USER_VAL;
			}
		}
		else return DATAFRAMES_MISMATCH;
	}
	else return RECV_EERROR; //Data Not received
	
	return MALFUNCTION;
}

int Find_user(char *target)
{
	int idx = -1;
	for(int i = 0;i<MAX_USER;i++)
	{
		if(strcmp(user_ID[i], target) == 0)
		{
			idx = i;
			break;
		}
	}
	return idx;
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
void Group_Chatting(int sockfd)
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