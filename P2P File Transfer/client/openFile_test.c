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

#define USER_036

#ifdef  USER_025
#define P2P_SHARE_DIR_PATH "/home/st2021146025/P2P_shared_files"
#else
#define P2P_SHARE_DIR_PATH "/home/st2021146036/P2P_shared_files"
#endif

int main(void){
    
    FILE *fp;
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", P2P_SHARE_DIR_PATH, "TUK.txt");
    fp = fopen(file_path, "r");
    if(fp == NULL){
        perror("fopen error");
        return -1;
    }
    char buffer[512];
    while(!feof(fp)){
        if(fgets(buffer, sizeof(buffer), fp) != NULL){
            printf("%s", buffer);
        }
    }
    fclose(fp);
    return 0;
}