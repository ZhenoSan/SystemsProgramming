#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_READ_SIZE 1024
#define MAX_WRITE_SIZE 4096

#define SERR STDERR_FILENO
#define SIN STDIN_FILENO
#define SOUT STDOUT_FILENO
#define TRUE 1
#define FALSE 0
#define handle_error(msg) \
               do { perror(msg); exit(EXIT_FAILURE); } while (0) 
#define PRINT_CMD write(SOUT,  "\x1B[36mCMD: \x1B[0m", strlen("\x1B[36m CMD: \x1B[0m"));    
               
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

typedef int BOOL;

int isNumeric(const char *s)
{
    while (*s) {
        if (isdigit(*s++) == 0) return 0;
    }
    return 1;
}

void * Read_From_Sock(void* sock_add){
	char rbuff[MAX_READ_SIZE];
	int rcount;
	label:
	while(*((int *)sock_add)==-1){
		sleep(1);
		Read_From_Sock(sock_add);
		return NULL;
	}
	while(1){
		if(*((int *)sock_add)==-1) goto label;
		if((rcount = read(*((int *)sock_add), rbuff, sizeof(rbuff)))==-1)
			handle_error("Sock Read Err");
		if(rcount ==0){
			close(*((int *)sock_add));
			*((int *)sock_add) = -1;
			rcount = sprintf(rbuff, "Server Disconnected.\n");
		}
		write(SOUT,  "\x1B[35mOUTPUT: \x1B[0m", strlen("\x1B[35mOUTPUT: \x1B[0m"));
		if(write(SOUT, rbuff, rcount)<0) 
			handle_error("Scrn Write Err");
		PRINT_CMD
		rbuff[0]='\0';
	}
}

int main(int argc, char *argv[]){
	int sock = -1;
	pthread_t read_thread, write_thread;
	static struct sockaddr_in server;
	if(argc==2 && (isNumeric(argv[1]))){
		inet_pton(AF_INET,"127.0.0.1", &(server.sin_addr));
		if((sock = socket(AF_INET, SOCK_STREAM, 0))==-1)
			handle_error("Opening Sock Err"); 	
		server.sin_port = htons(atoi(argv[1]));
		server.sin_family = AF_INET;
		//server.sin_addr.s_addr = INADDR_ANY;
		bzero(&server.sin_zero, 8);
		
		if (connect(sock,(struct sockaddr *) &server,sizeof(server)) == -1){ 
			perror("Conn Stream Sock");
			sock = -1;
		}else{
			char ConnMsg[64];	
			int ConnMsgLen = sprintf(ConnMsg, "Connected with %s:%d\n", inet_ntoa(server.sin_addr), server.sin_port);
			write(SOUT, ConnMsg, ConnMsgLen);
			PRINT_CMD
		}
	}
	pthread_create(&read_thread, NULL, Read_From_Sock, &sock);
	char wbuff[MAX_WRITE_SIZE];
	int count;
	while(1){
		if(sock==-1)
			PRINT_CMD
		if((count = read(SIN, wbuff, sizeof(wbuff)))==-1)
			handle_error("Scrn Read Err");
		wbuff[count]='\0';
		if(!strncmp(wbuff, "con", 3) && sock==-1){
			char* tok = strtok(wbuff, " \n");
			if((tok = strtok(NULL, " \n"))==NULL){
				write(SOUT, "Invalid IP\n",strlen("Invalid IP\n")); 
				continue;
			}
			inet_pton(AF_INET, tok, &(server.sin_addr));
			if((tok = strtok(NULL, " \n"))==NULL){
				write(SOUT, "Invalid Port\n",strlen("Invalid Port\n")); 
				continue;
			}
			if((sock = socket(AF_INET, SOCK_STREAM, 0))==-1)
				handle_error("Opening Sock Err"); 	
			server.sin_port = htons(atoi(tok));
			server.sin_family = AF_INET;
			//server.sin_addr.s_addr = INADDR_ANY;
			bzero(&server.sin_zero, 8);
			
			if (connect(sock,(struct sockaddr *) &server,sizeof(server)) == -1){ 
				perror("Conn Stream Sock");
				sock = -1;
				continue;
			}
			char ConnMsg[64];	
			int ConnMsgLen = sprintf(ConnMsg, "Connected with %s:%d\n", inet_ntoa(server.sin_addr), server.sin_port);
			write(SOUT, ConnMsg, ConnMsgLen);
			PRINT_CMD
			continue;
		}
		if(sock == -1){
			if(!strncmp(wbuff, "exit",4))
				exit(EXIT_SUCCESS); 
			write(SOUT, "Not Connected.\n", strlen("Not Connected.\n")); 
			continue;
		}
		if(write(sock,wbuff,count)==-1 && sock!=-1)
			handle_error("Sock Write Err");
		if(!strncmp(wbuff, "discon",6)){
			close(sock);
			sock = -1;
		}
		if(!strncmp(wbuff, "exit", 4)){
			sleep(1);
			write(SOUT, "\n", 1);
			exit(EXIT_SUCCESS);
		}
		wbuff[0]='\0';
	}
}
