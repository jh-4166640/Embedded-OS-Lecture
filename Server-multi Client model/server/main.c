#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERV_IP		"220.149.128.92"
#define SERV_PORT	4000
#define BACKLOG		10

#define INIT_MSG "=========================\nHello! I'm P2P File Sharing Server\nPlease, LOG-IN!\n=========================\n"
#define LOG_IN_SUCCESS(msg,user) sprintf(msg,"Log-in success!! [%s] *^^*",user)
#define LOG_IN_FAIL				 "Log-in fail: Incorrect password..."
#define NOT_FIND_USER			 "Not Find user ID"
#define MAX_USER 3
#define MAX_LEN 64
char *user_ID[MAX_USER]= {"user1","user2"};
char *user_PW[MAX_USER]= {"passwd1","passwd2"};

void Client_Log_in(int client_fd);
int Find_user(char * target);



int main(void)
{
	int sockfd, new_fd;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	unsigned int sin_size;
	
	/* buffer */
	int rcv_byte;
	char buf[512];
	char id[20];
	char pw[20];
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
	else printf("listen() is OK...\n");


	while(1)
	{
		sin_size = sizeof(struct sockaddr_in);
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);	
		if(new_fd < 0)
		{
			perror("accept() error lol!");
			continue;
		}
		send(new_fd , INIT_MSG, strlen(INIT_MSG)+1,0);
		int pid=fork();
		if(pid<0)
		{
			perror("fork() error");
			close(new_fd);
			continue;
		}
		else if(pid == 0) // child process
		{
			rcv_byte=recv(new_fd,buf,sizeof(buf),0);
			if(rcv_byte>0)
			{
				char * pdiv = strchr(buf,'|');
				int div_idx = 0;
				int user_idx = -1;
				if(pdiv != NULL)
				{
					div_idx = pdiv-buf;
					strncpy(id, buf,div_idx);
					id[div_idx]='\0';
					strcpy(pw, pdiv+1);
					user_idx = Find_user(id);

					printf("=========================\nUser Information\n");
					printf("ID: %s, PW: %s\n",id,pw);
					printf("=========================\n");
					

					if(user_idx >= 0)
					{
						if(strcmp(user_PW[user_idx],pw) == 0) // Log in success
						{
							LOG_IN_SUCCESS(msg,id);
							send(new_fd , msg, strlen(msg)+1,0);
							printf("%s\n\n",msg);
						}
						else // Log in fail
						{
							send(new_fd , LOG_IN_FAIL, strlen(LOG_IN_FAIL)+1,0);
							printf("%s\n\n",LOG_IN_FAIL);
						}
					}
					else // Not found user ID
					{
						send(new_fd , NOT_FIND_USER, strlen(NOT_FIND_USER)+1,0);
						printf("%s\n\n",NOT_FIND_USER);
					}
				}
			}
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

void Client_Log_in(int client_fd)
{
	char buf[512];
	char id[20];
	char pw[20];
	
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