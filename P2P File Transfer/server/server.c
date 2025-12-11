#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/wait.h>

#define SERV_IP      "220.149.128.92"
#define SERV_PORT   4467 // 고정
#define BACKLOG      10
/* Client_Log_in return code */
#define MALFUNCTION       		-2
#define RECV_EERROR       		-1
#define RECV_NO_ERROR       	0
#define DATAFRAMES_MISMATCH 	-3

#define INIT_MSG "=========================\nHello! I'm P2P File Sharing Server\nPlease, LOG-IN!\n=========================\n"

#define LOG_IN_SUCCESS_VAL		1
#define LOG_IN_FAIL_VAL         2
#define NOT_FIND_USER_VAL      	3

#define LOG_IN_SUCCESS(msg,user) 		sprintf(msg,"Log-in success [%s]",user)
#define LOG_IN_SUCCESS_STR(msg, user) 	sprintf(msg,"Welcome to the TUlk [%s]",user)
#define LOG_IN_FAIL             		"Log-in fail: Incorrect password..."
#define NOT_FIND_USER 		         	"Not Find user ID"
#define CHAT_INIT 						"----welcome chating room---\n"
#define ENTER_USER_BROADCAST(msg,user) 	sprintf(msg,"[%s] entered the chat room.\n",user)


/*share memory size*/
#define MAX_USER 		3
#define MSG_SIZE 		512
#define MSG_BUFFER_SIZE 256


#define FILE_P2P_ON 1
#define FILE_P2P_OFF 0

/*share memory and semaphore key value*/
#define SHM_KEY 7878
#define SEM_KEY 5678

char EXIT_FLAG[MAX_USER]={0,};

char *user_ID[MAX_USER]= {"user1","user2","user3"};
char *user_PW[MAX_USER]= {"passwd1","passwd2","passwd3"};

char user_IP[MAX_USER][64];
char user_PORT[MAX_USER][16];

#define DATA_NOT_RECEIVED -1

void Send_Message(int sockfd, char *msg);
int Recv_Message(int sockfd, char *buf);
void Group_Chatting_Process(int sockfd, char *msg, char *buf);

int Client_Log_in(int client_fd,char *buf,int *user_num);
int Find_user(char * target);

void* shared_memory_write_thread(void* arg);
void* shared_memory_read_thread(void* arg);

/* share memory */
struct share_memory{
	char msg[MSG_BUFFER_SIZE][MSG_SIZE];
	int write_idx;
	int read_idx[MAX_USER];
};
struct thread_arg {
	struct share_memory* sh;
	int user_id;
	int sockid;
};
//semaphore id 전역변수로 설정
int semid;


union semun{
   int val;
   struct semid_ds *buf;
   unsigned short *array;
};
int main(void)
{
	int sockfd, new_fd;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	unsigned int sin_size;

	/* buffer */
	int rcv_byte;
	char buf[512];
	int val = 1;

	/* ======share memory and semaphore 생성======= */
	int shmid= shmget(SHM_KEY, sizeof(struct share_memory),IPC_CREAT | 0666);
	if (shmid < 0) {
		perror("shmget");
		exit(1);
	}
	struct share_memory* sh_data=(struct share_memory*) shmat(shmid,NULL,0);
		
	sh_data->write_idx = 0;
	for (int i = 0; i < MAX_USER; i++)
		sh_data->read_idx[i] = 0;
	for (int i = 0; i < MSG_BUFFER_SIZE; i++) {
		memset(sh_data->msg[i], 0, MSG_SIZE);
	}
	if (sh_data == (void*)-1) {
		perror("shmat");
		exit(1);
	}
	semid= semget(SEM_KEY,1,IPC_CREAT | 0666);
	union semun su;
	su.val = 1;
	semctl(semid,0,SETVAL,su);
	/* ========================================== */
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
			int user_num;
			send(new_fd , INIT_MSG, strlen(INIT_MSG)+1,0);
			int receive_res = Client_Log_in(new_fd,buf,&user_num);
			switch(receive_res)
			{
			case LOG_IN_SUCCESS_VAL:
				pthread_t tid_write,tid_read;
				int ret;
				printf("%s",CHAT_INIT);
				Send_Message(new_fd,CHAT_INIT);

				//지역변수임으로 스레드에 사용하기 위해 malloc 사용
				struct thread_arg* arg = malloc(sizeof(struct thread_arg));
				arg->sh = sh_data;
				arg->user_id = user_num;
				arg->sockid = new_fd;
				EXIT_FLAG[user_num]=0;
				ret=pthread_create(&tid_write, NULL,shared_memory_write_thread,arg);
				if (ret != 0) {
					fprintf(stderr, "pthread_create(write_thread) 실패: %s\n", strerror(ret));
					exit(1);
				}
				ret=pthread_create(&tid_read,NULL,shared_memory_read_thread,arg);
				if (ret != 0) {
					fprintf(stderr, "pthread_create(read_thread) 실패: %s\n", strerror(ret));
					exit(1);
				}
				pthread_join(tid_write,NULL);
				pthread_join(tid_read,NULL);
				
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
			//exit(0);
		}
		else if(pid > 0) // parent process
		{
			
			close(new_fd);   
			
		}
	}
	exit(0);

	close(new_fd);
	close(sockfd);

	return 0; 
}


int Client_Log_in(int client_fd, char *buf,int *user_num)
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
			*user_num=user_idx;
			//  printf("=========================\nUser Information\n");
			//  printf("ID: %s, PW: %s\n",id,pw);
			//  printf("=========================\n");
			
			if(user_idx >= 0)
			{
				if(strcmp(user_PW[user_idx],pw) == 0) // Log in success
				{
					char temp[64];
					char send_temp[64];
					char recv_ip_port[80];
					LOG_IN_SUCCESS(temp,id);
					printf("%s\n\n",temp);

					LOG_IN_SUCCESS_STR(send_temp, id);
					sprintf(msg,"%s|%d",send_temp,LOG_IN_SUCCESS_VAL);
					send(client_fd , msg, strlen(msg)+1,0);
					//printf("%s\n\n",msg);
					Recv_Message(client_fd, user_IP[user_idx]); // receive P2P IP
					Recv_Message(client_fd, user_PORT[user_idx]); // receive P2P PORT
					sprintf(recv_ip_port,"IP %s| port %s",user_IP[user_idx],user_PORT[user_idx]);
					//printf("P2P IP and PORT received: %s\n\n",recv_ip_port);
					for(int i = 0; i<2;i++)
						printf("user_IP[%d]: %s, user_PORT[%d]: %s\n",i,user_IP[i],i,user_PORT[i]);

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

//세마포 연산
void P(int semid)
{
    struct sembuf p = {0, -1, 0};  // 0번 세마포에 대해 -1
    if (semop(semid, &p, 1) == -1) {
        perror("P semop");
        exit(1);
    }
}

void V(int semid)
{
    struct sembuf v = {0, +1, 0};  // 0번 세마포에 대해 +1
    if (semop(semid, &v, 1) == -1) {
        perror("V semop");
        exit(1);
    }
}
//write thread
void* shared_memory_write_thread(void* arg){
	static int first = 0;
    struct thread_arg* k=(struct thread_arg*)arg;
	struct share_memory* sh_data = k->sh;
	int user_num=k->user_id;
	int sockid=k->sockid;
   	int ret;
   	char input_buf_th[512];
   	char server_display[512];
   	if(first==0){
		first=1;
	  	ENTER_USER_BROADCAST(server_display,user_ID[user_num]);
	  	P(semid);
	  	strncpy(sh_data->msg[sh_data->write_idx], server_display, MSG_SIZE - 1);
	  	sh_data->msg[sh_data->write_idx][MSG_SIZE - 1] = '\0';
	  	sh_data->read_idx[user_num] = (sh_data->read_idx[user_num] + 1) % MSG_BUFFER_SIZE;
		sh_data->write_idx = (sh_data->write_idx + 1) % MSG_BUFFER_SIZE;
		V(semid);
    }
   	while(1){
	  	ret=Recv_Message(sockid,input_buf_th);
    	if(ret==DATA_NOT_RECEIVED){
			sprintf(server_display,"[%s] exit...\n",user_ID[user_num]);
			perror(server_display); // server Log 용
			P(semid);
			strncpy(sh_data->msg[sh_data->write_idx], server_display, MSG_SIZE - 1);
			sh_data->msg[sh_data->write_idx][MSG_SIZE - 1] = '\0';
			sh_data->read_idx[user_num] = (sh_data->read_idx[user_num] + 1) % MSG_BUFFER_SIZE;
			sh_data->write_idx = (sh_data->write_idx + 1) % MSG_BUFFER_SIZE;
			V(semid);

			
			exit(1);
    	}
		if(strcmp(input_buf_th,"exit")==0){
			EXIT_FLAG[user_num]=1;
			free(arg);
			break;
		}
		sprintf(server_display,"[%s] %s",user_ID[user_num],input_buf_th);
		printf("%s\n",server_display);

		P(semid);
		strncpy(sh_data->msg[sh_data->write_idx], server_display, MSG_SIZE - 1);
		sh_data->msg[sh_data->write_idx][MSG_SIZE - 1] = '\0';
		sh_data->read_idx[user_num] = (sh_data->read_idx[user_num] + 1) % MSG_BUFFER_SIZE;
		sh_data->write_idx = (sh_data->write_idx + 1) % MSG_BUFFER_SIZE;
		V(semid);
   	}   
	return NULL;
}
//read thread
void* shared_memory_read_thread(void* arg){
   	struct thread_arg* k=(struct thread_arg*)arg;
	struct share_memory* sh_data = k->sh;
	int user_num=k->user_id;
	int sockid=k->sockid;
	int r, w;
	char *command_loc;
	int command_idx;
	
	char target_id[32];
	char receive_id[10];
	int target_user_num=-1;
	int receive_user_num=-1;
	char input_buf_th[512];
	char transmit_ip_port[50];

	int file_op_flag=FILE_P2P_OFF;
   	while(1){
		receive_id[0] = '\0';
		target_id[0] = '\0';
		if(EXIT_FLAG[user_num]==1){
			EXIT_FLAG[user_num]=0;
			break;
		}
		P(semid);
		r = sh_data->read_idx[user_num];
		w = sh_data->write_idx;
		V(semid);
		if(w != r){
			strncpy(input_buf_th, sh_data->msg[r], sizeof(input_buf_th)-1);
			input_buf_th[sizeof(input_buf_th)-1] = '\0';
			command_loc=strchr(input_buf_th,'$');
			if(command_loc != NULL){
				
				command_idx=command_loc-input_buf_th;

				if(strncmp(input_buf_th+command_idx,"$FILE|",6)==0){
				
					file_op_flag=FILE_P2P_ON;
					char *tid = input_buf_th + command_idx + 6;
					strncpy(target_id, tid, sizeof(target_id)-1);
					target_id[sizeof(target_id)-1] = '\0';

					char *right_bracket = strchr(input_buf_th, ']');
					if (right_bracket != NULL) {

						int username_len = right_bracket - (input_buf_th + 1);
						strncpy(receive_id, input_buf_th + 1, username_len);
						receive_id[username_len] = '\0';   // NULL terminate
					}

				}
			}
			
			switch(file_op_flag){
			case FILE_P2P_ON:
				file_op_flag=FILE_P2P_OFF;
				receive_user_num=Find_user(receive_id);
				target_user_num=Find_user(target_id);
				printf("rec_user %d tar_user %d\n",receive_user_num,target_user_num);
				if(target_user_num==-1)
				{
					// Send_Message(sockid,NOT_FIND_USER);
					sh_data->read_idx[user_num] = (r + 1) % MSG_BUFFER_SIZE;
					break;
				}
				if(target_user_num==user_num){
					sprintf(transmit_ip_port,"$FILE|%s|%s|%s\n",target_id,user_IP[receive_user_num],user_PORT[receive_user_num]);
					printf("FILE_P2P_ON %s\n",transmit_ip_port);
					Send_Message(sockid,transmit_ip_port);
					sh_data->read_idx[user_num] = (r + 1) % MSG_BUFFER_SIZE;
				}
				else{
					
					sh_data->read_idx[user_num] = (r + 1) % MSG_BUFFER_SIZE;
				}

				break;
			case FILE_P2P_OFF:
			default:
				Send_Message(sockid,input_buf_th);
				sh_data->read_idx[user_num] = (r + 1) % MSG_BUFFER_SIZE;
				break;	
			}
   		}
	}
   	return NULL;   
}