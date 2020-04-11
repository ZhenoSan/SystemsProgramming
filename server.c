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
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>

#define MAX_READ_SIZE 4096
#define MAX_WRITE_SIZE 1024

#define SERR STDERR_FILENO
#define SIN STDIN_FILENO
#define SOUT STDOUT_FILENO
#define HELP "List Of Possible Commands:\nadd [num 1] [num 2] ..\nsub [num 1] [num 2] ..\ndiv [num 1] [num 2] ..\nmul [num 1] [num 2] ..\nrun [program]\nlist [all] Show Running/All Programs\nkill [Process ID]\nexit Close Program\n"
#define TRUE 1
#define FALSE 0
#define handle_error(msg) \
               do { perror(msg); exit(EXIT_FAILURE); } while (0)

//COLOR DEFINITIONS
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

typedef int BOOL;
typedef struct _proclist 
{ 
	char *ProcName;
	int Sno;
	time_t StartTime;
	time_t EndTime;
	int ProcId;
	BOOL isActive;
	struct _proclist *next;
} ProcList;

struct _conlist
{
	char IP[15];
	int port;
	pid_t pid;
	int cfd;
	int prfd[2];
	int pwfd[2];
	int status;
};

ProcList *start = NULL;
struct _conlist ConList[10];
int Curr_Con = 0;

void ToggleProc(ProcList **head, int pid, BOOL rmv);

static void serv_sig_handler(int signo){
	//int Status;
	int child_pid = wait(NULL);
	for(int i=0;i<Curr_Con;i++){
		if(child_pid == ConList[i].pid){
			ConList[i].status = 0;
			break;
		}
	}		
}

static void sig_handler(int signo){
	//int Status;
	int child_pid = wait(NULL);
	ToggleProc(&start,child_pid, FALSE);		
}

int isNumeric(const char *s)
{
    while (*s) {
        if (isdigit(*s++) == 0) return 0;
    }
    return 1;
}

void InsertProc(ProcList **head, char *pname, int pid){
	int count=1;
	ProcList **tracer = head;
	ProcList *newp;
	newp = (ProcList *) malloc (sizeof(ProcList));
	newp -> ProcName = (char *)malloc(strlen(pname) +1);
	strcpy(newp -> ProcName, pname);
	newp -> ProcId = pid; // (*(newp)).ProcId = pid;
	newp -> isActive = TRUE;
	newp -> StartTime = time(NULL);
	newp -> EndTime = 0;
	newp -> next = NULL;
	while((*tracer)){ //&& (*tracer)->ProcId<pid)
		tracer = &(*tracer)->next;
		count++;
	}
	newp -> Sno = count;
	newp -> next = *tracer;
	*tracer = newp;
}

void ToggleProc(ProcList **head, int pid, BOOL rmv){
	BOOL present = FALSE;
	ProcList *old;
	ProcList **tracer = head;
	
	while((*tracer) && !(present = pid==(*tracer)->ProcId ))
		tracer = &(*tracer)->next;

	if(present){
		old = *tracer;
		old -> EndTime = time(NULL); 
		old -> isActive = FALSE;	
		if(rmv){
			*tracer = (*tracer)->next;
			free(old -> ProcName);
			free(old); // free up remainder of list element 
		}
	}
}

static int newproc(char MsgArr[]){
	char* etok = strtok(NULL, " \n");
	int pipefd[2];
	char* tbuff;
	//pipe2(pipefd,FD_CLOEXEC);
	pipe(pipefd);
	if(etok==NULL){
		return strlen(strcpy(MsgArr, "Enter Program Name\n"));
	}
	int child_pid = fork();
	if(child_pid){
		close(pipefd[1]);
		if(read(pipefd[0],tbuff,1)==0){
			InsertProc(&start, etok, child_pid);
			return strlen(strcpy(MsgArr, "Program Opened\n"));
		}else{ 
			return strlen(strcpy(MsgArr, "Program Execution Failed\n"));
		}
	}
	else{
		close(pipefd[0]);
		fcntl(pipefd[1],F_SETFD, FD_CLOEXEC);
		if(execlp(etok, etok, NULL)==-1){
			write(pipefd[1],"\n",1);
			exit(99);
		}
		return 1;
	}
}

static int killproc(char MsgArr[]){
	char* tok = strtok(NULL, " \n");
	BOOL present = FALSE;
	ProcList *old;
	ProcList **tracer = &start;
	if(tok==NULL) tok="0";
	if(!strcmp(tok, "all")){
		while(*tracer){
			kill((*tracer)->ProcId, SIGTERM);
			tracer = &(*tracer)->next;
		}
		return strlen(strcpy(MsgArr, "All Programs Killed\n"));
	}
	else if(isNumeric(tok)){
		int pid = atoi(tok);
		while((*tracer) && !(present = pid==(*tracer)->ProcId ))
			tracer = &(*tracer)->next;
		if(present){
			kill(pid, SIGTERM);
			return strlen(strcpy(MsgArr, "Program Killed\n"));
		}
	return strlen(strcpy(MsgArr, "Process ID not in list.\n"));
	}
	else{
		int flag = 0;
		check_active:
		while((*tracer)&& !((present =(!strcmp(tok,(*tracer)->ProcName)) && (*tracer)->isActive==TRUE)))
			tracer = &(*tracer)->next;
		if(present){
			kill((*tracer)->ProcId, SIGTERM);
			return strlen(strcpy(MsgArr, "Program Killed\n"));		
		}else{
			return strlen(strcpy(MsgArr, "Process Is Not In The List.\n"));			
		}
	}
	return strlen(strcpy(MsgArr, "You Don't Have Permission To Kill This Process.\n"));
}

static int printlist(char ListStr[]){
	char* tok = strtok(NULL, " \n");
	ProcList **tracer = &start;/*
	while(*tracer) {
		if((*tracer) -> isActive){
			//waitpid((*tracer)->ProcId, NULL,WNOHANG);
			if(kill((*tracer)->ProcId,0))
				ToggleProc(&start, (*tracer)->ProcId, FALSE);
		}tracer = &(*tracer)->next;
	}*/
	tracer = &start;
	int strsize =0;
	char ListItemStr[128];
	struct tm * ts;
	ListStr[0] ='\0';
	if(tok==NULL){
		strsize = strlen(strcat(ListStr, "\nSr no. : Process ID : Process Name\n"));
		int temp = strsize;
		while (*tracer) {
			if((*tracer) -> isActive){
				int lstcount = sprintf(ListItemStr, "   %d   :    %d    : %s\n", (*tracer)->Sno, (*tracer)->ProcId, (*tracer)->ProcName);
				strncat(ListStr, ListItemStr, lstcount);
				strsize +=lstcount;
			}tracer = &(*tracer)->next;
		}
		if(strsize==temp){ 
			strsize= strlen(strcpy(ListStr, "No Process Currently Running!\n"));
		}
	}
	else if(!strcmp(tok, "all")){
		strsize = strlen(strcat(ListStr, "\nSr no. : Process ID : Status : Start Time : End Time : Elapsed Time : Process Name\n"));
		int lstcount =0;
		int temp = strsize;
		time_t elapsed_time = 0;
		while (*tracer) {
			ts = localtime(&((*tracer)->StartTime));
			int hr = ts->tm_hour;
			int mm = ts->tm_min;
			int ss = ts->tm_sec;
			if((*tracer)->EndTime){
				elapsed_time = (*tracer)->EndTime - (*tracer)->StartTime;
				ts = localtime(&((*tracer)->EndTime));
				lstcount = sprintf(ListItemStr, "   %d   :    %d    :   %d    :  %u:%u:%u  : %u:%u:%u :      %ld(s)    : %s\n", 
						(*tracer)->Sno, (*tracer)->ProcId, (*tracer)->isActive, hr, mm, ss, ts->tm_hour, ts->tm_min, 
							ts->tm_sec, elapsed_time,(*tracer)->ProcName);
			}else{
				elapsed_time = time(NULL) - (*tracer)->StartTime;
				lstcount = sprintf(ListItemStr, "   %d   :    %d    :   %d    :  %u:%u:%u  : Running  :      %ld(s)    : %s\n", (*tracer)->Sno, 
					(*tracer)->ProcId, (*tracer)->isActive, hr, mm, ss, elapsed_time, (*tracer)->ProcName);	
			}
			strncat(ListStr, ListItemStr, lstcount);
			strsize +=lstcount;
			tracer = &(*tracer)->next;
		}
		if(strsize==temp){ 
			strsize = strlen(strcpy(ListStr, "No Process Currently Loaded!\n"));
		}
	}
	else strsize = strlen(strcpy(ListStr, "SYNTAX: list [all]\n"));
	return strsize;
}

static int calc(int operation, char sumStr[]){
	int operand[2048];
	operand[0]=0;
	int i=0,j=1, tcount=0;
	double sum =0;
	char* ctok = strtok(NULL, " \n");
	while(ctok!=NULL){
		if(!isNumeric(ctok)) break;
		operand[i] = atoi(ctok);
		ctok = strtok(NULL, " \n");
		i++;
	}
	sum = operand[0];
	while(i>j){
		switch(operation){
			case 1:
				sum += operand[j];
				break;
			case 2:
				sum -= operand[j];
				break;
			case 3:
				sum *= operand[j];
				break;
			case 4:
				if(!operand[j])
					return sprintf(sumStr, "Division By Zero :(\n");
				sum /= operand[j];
				break;
		}
		j++;
	}
	switch(operation){
			case 1:
				return sprintf(sumStr, "Result Of Addition: %d\n", (int)sum);
			case 2:
				return sprintf(sumStr, "Result Of Subtraction: %d\n", (int)sum);
			case 3:
				return sprintf(sumStr, "Result Of Multiplication: %d\n", (int)sum);
			case 4:
				return sprintf(sumStr, "Result Of Division: %.3f\n", sum);
	}
	return tcount;
}
void * Server_Command(){
	char cmdBuff[64];
		int count;
		while(1){
			if((count = read(SIN, cmdBuff, 64))==-1)
				handle_error("Server Term Read");
			cmdBuff[count-1] = '\0';
			if(!strcmp(cmdBuff, "exit")){
					kill(0, SIGTERM);
					exit(1);
			}
		}
}

int main(int argc, char *argv[]){
	if(argc!=2 || !(isNumeric(argv[1]))){
		write(SOUT, "Usage: ./servms [port]\n", strlen("Usage: ./servms [port]\n"));
		exit(1); 
	}
	signal(SIGCHLD, serv_sig_handler);
	struct sockaddr_in server, client;
	struct pollfd fds[2];
	int timeout = (-1 * 60 * 1000);
	int sockfd, clifd, rval, ConnMsgLen, len;
	char ConnMsg[128];
	char* token;
	pthread_t server_cmd_thrd;
	
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0))==-1)
		handle_error("Opening Stream Sock");
		
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(atoi(argv[1]));
	bzero(&server.sin_zero, 8);
	len = sizeof(server);
	
	if(bind(sockfd, (struct sockaddr *)&server, len)==-1)
		handle_error("Building Stream Sock");
	
	ConnMsgLen = sprintf(ConnMsg, "Server starting with %s:%d\n", inet_ntoa(server.sin_addr), server.sin_port);
	write(SOUT, ConnMsg, ConnMsgLen);
	listen(sockfd, 1);
	//pthread_create(&server_cmd_thrd, NULL, Server_Command, NULL);
	fds[0].fd = SIN;
	fds[0].events = POLLIN;
	fds[1].fd = sockfd;
	fds[1].events = POLLIN;
	while(1){
		int rc = poll(fds, 2, timeout);
		if (rc==-1)
			if(errno != EINTR)
				handle_error("Poll");
			else continue;
		if (rc == 0){
		  write(SOUT, "Poll() Timed Out", strlen("Poll() Timed Out"));
		  break;
		}
		if(fds[0].revents == POLLIN){
			char cmdBuff[64]; // server side commands
			char cmdBuffCpy[64];
			int count;
			if((count = read(SIN, cmdBuff, 64))==-1)
				handle_error("Server Term Read");
			cmdBuff[count] = '\0';
			strcpy(cmdBuffCpy, cmdBuff);
			char* cmdBuffTok = strtok(cmdBuff, " \n");
			if(cmdBuffTok==NULL) write(SOUT, "CMD:", strlen("CMD:")); 
			else if(!strcmp(cmdBuffTok, "exit")){
					kill(0, SIGTERM);
					//exit(1);
			}else if(!strcmp(cmdBuffTok, "list")){	
				cmdBuffTok = strtok(NULL, " \n");
				if(cmdBuffTok==NULL) write(SOUT, "INVALID SYNTAX\n", strlen("INVALID SYNTAX\n")); 
				else if(!strncmp(cmdBuffTok, "con", 3)){
					char ConListArr[128];
					if(Curr_Con>0)
						write(SOUT, "Sr No. : Status : IP Address : Port  : Proc ID\n", strlen("Sr No. : Status : IP Address : Port  : Proc ID\n"));
					else
						write(SOUT, "No Connections Established.\n", strlen("No Connections Established.\n"));
					for(int j =0; j<Curr_Con;j++){
						write(SOUT, ConListArr, sprintf(ConListArr, "   %d   :   %d    :  %s : %d :%d\n", j+1, ConList[j].status, ConList[j].IP, ConList[j].port, ConList[j].pid));
					}
				}
				else if(!strncmp(cmdBuffTok, "all", 3)){
					char ServOutput[MAX_WRITE_SIZE];
					if(Curr_Con>0)
						write(SOUT, "\nSr no. : Process ID : Status : Start Time : End Time : Elapsed Time : Process Name\n", strlen("\nSr no. : Process ID : Status : Start Time : End Time : Elapsed Time : Process Name\n"));
					else
						write(SOUT, "No Processes Running.\n", strlen("No Processes Running.\n"));
					for(int j=0;j<Curr_Con;j++){
						if(!ConList[j].status) continue;
						write(ConList[j].pwfd[1], "list all\n", 9);
						int temp_r = read(ConList[j].prfd[0], ServOutput, sizeof(ServOutput));
						if(temp_r>83)
							write(SOUT, &ServOutput[84], temp_r-84);
					}
				}else{
					if(count <10 ){ 
						write(SOUT, "SYNTAX: List <all|conn|IP> <Port>\n", strlen("SYNTAX: List <all|conn|IP> <Port>\n"));
						continue;
					}
					char ServOutput[MAX_WRITE_SIZE];
					char *nbuff = strtok(NULL, ": \n");;
					for(int j=0;j<Curr_Con;j++){
						int str_len;
						if(strlen(ConList[j].IP)< strlen(cmdBuffTok)){
							str_len = strlen(ConList[j].IP);
						}else str_len = strlen(cmdBuffTok);
						if(!strncmp(ConList[j].IP, cmdBuffTok, str_len)){
							if(nbuff !=NULL){
								if(ConList[j].port==atoi(nbuff)){
									write(ConList[j].pwfd[1], "list all\n", 9);
									int temp_r = read(ConList[j].prfd[0], ServOutput, sizeof(ServOutput));
									write(SOUT, ServOutput, temp_r);
								}
							}else
								write(SOUT, "SYNTAX: List <all|conn|IP> <Port>\n", strlen("SYNTAX: List <all|conn|IP> <Port>\n"));
								continue; 
						}
					}
				}
			}else if(!strncmp(cmdBuffTok, "msg", 3)){
				cmdBuffTok = strtok(NULL, " \n");
				if(cmdBuffTok !=NULL){
					if(!strncmp(cmdBuffTok, "all", 3)){
						for(int j=0;j<Curr_Con;j++){
							if(!ConList[j].status) continue;
							write(ConList[j].pwfd[1], &cmdBuffCpy[8], count-8);
						}
						write(SOUT, "OUTPUT: Message Broadcasted\n", strlen("OUTPUT: Message Broadcasted\n"));	
					}
					int str_len = 0;
					int socket_len;
					char* port_no = strtok(NULL, " :");//Get Port
					if(port_no==NULL)
						write(SOUT, "ERROR: INVALID PORT\n", strlen("ERROR: INVALID PORT\n"));
					else{
						int j=0;
						for(;j<Curr_Con;j++){
							socket_len = strlen(cmdBuffTok);
							if(strlen(ConList[j].IP)< strlen(cmdBuffTok)){
								str_len = strlen(ConList[j].IP);
							}else str_len = strlen(cmdBuffTok);
							if(!strncmp(ConList[j].IP, cmdBuffTok, str_len)){
								socket_len += strlen(port_no)+2;
								if(ConList[j].port==atoi(port_no)){
									if(!ConList[j].status) break;
									write(SOUT, "OUTPUT: Message Sent\n", strlen("OUTPUT: Message Sent\n"));
									write(ConList[j].pwfd[1], &cmdBuffCpy[socket_len+4], count-socket_len-4);
									break;
								}
							}
						}
						if(j==Curr_Con) write(SOUT, "Port or IP Invalid\n", strlen("Port or IP Invalid\n"));
					}
				}else
					write(SOUT, "SYNTAX: MSG <ALL | IP> <PORT> <Msg>\n", strlen("SYNTAX: MSG <ALL | IP> <PORT> <Msg>\n"));
			}else
				write(SOUT, "ERROR: INVALID COMMAND\n", strlen("ERROR: INVALID COMMAND\n"));
		}
		if(fds[1].revents == POLLIN){
			if((clifd = accept(sockfd, (struct sockaddr *)&client, &len))==-1)
				handle_error("Accepting Client");
			strcpy(ConList[Curr_Con].IP, inet_ntoa(client.sin_addr));
			ConList[Curr_Con].port=client.sin_port;
			ConList[Curr_Con].cfd = clifd;
			ConList[Curr_Con].status = 1;
			pipe(ConList[Curr_Con].prfd);//Server Read
			pipe(ConList[Curr_Con].pwfd);//Server Write
			Curr_Con++;
			ConnMsgLen = sprintf(ConnMsg, GRN "Connected with %s:%d\n" RESET, inet_ntoa(client.sin_addr), client.sin_port);
			write(SOUT, ConnMsg, ConnMsgLen);
			ConList[Curr_Con-1].pid = fork();
			if(!ConList[Curr_Con-1].pid){
				close(ConList[Curr_Con-1].pwfd[1]);
				close(ConList[Curr_Con-1].prfd[0]);
				signal(SIGCHLD, sig_handler);
				struct pollfd cfds[2];
				cfds[0].fd = clifd;
				cfds[0].events = POLLIN;
				cfds[1].fd = ConList[Curr_Con-1].pwfd[0];
				cfds[1].events = POLLIN;
				char output[MAX_WRITE_SIZE];
				char mainBuff[MAX_READ_SIZE];// = "";
				char *buff[MAX_READ_SIZE / 512]; //[32]= {"","","","","","","","","",""};
				int output_len=0;
				while(1){
					int nrc = poll(cfds, 2, timeout);
					if (nrc==-1){
						if(errno != EINTR)
							handle_error("Poll");
						else continue;
					}if (nrc == 0){
					  write(SOUT, "Poll() Timed Out", strlen("Poll() Timed Out"));
					  break;
					}
					if(cfds[0].revents == POLLIN){
						output[0] = '\0';
						int i =0;
						rval = read(clifd, mainBuff, sizeof(mainBuff));
						mainBuff[rval]='\0';
						buff[i] = strcat(strtok(mainBuff, ";\n\0"), "\0");
						while(buff[i]!=NULL){
							i++;
							buff[i] = strcat(strtok(NULL, ";\n\0"), "\0");
						}
						int j =0;
						printf("\n");
						//write(SOUT, buff[0], strlen(buff[0]));
						if(rval==-1)
							handle_error("Client Sock Read");
						else if(rval > 0){
							if(rval==1)j=0;
							while(j<i){
								ConnMsgLen = sprintf(ConnMsg, BLU "Received %d Bytes From %s:%d -- Command: %s\n" RESET, rval, inet_ntoa(client.sin_addr), client.sin_port, buff[j]);
								write(SOUT, ConnMsg, ConnMsgLen);
								//printf("%d\n",j);
								//write(SOUT, buff[j], strlen(buff[j]));
								token = strtok(buff[j], " \n");
								if(token==NULL) token="empty";
								if(!strcmp(token, "add"))		output_len =calc(1, output);
								else if(!strcmp(token, "sub"))		output_len =calc(2, output);
								else if(!strcmp(token, "mul"))		output_len =calc(3, output);
								else if(!strcmp(token, "div"))		output_len =calc(4, output);
								else if(!strcmp(token, "kill"))		output_len = killproc(output);
								else if(!strcmp(token, "run"))		output_len = newproc(output);
								else if(!strcmp(token, "list"))		output_len = printlist(output);
								else if(!strcmp(token, "help"))		output_len =strlen(strcpy(output, HELP));
								else if(!strcmp(token, "exit")||!strncmp(token, "discon",6)){
										strtok(strcpy(buff[j], "kill all"), " ");
										killproc(output);
										output_len = strlen(strcpy(output, "Client Disconnecting.\n"));
										write(clifd, output, output_len);
										goto Conn_End_Label;
								}
								else output_len = strlen(strcpy(output, "Write A Valid Command. Use help.\n"));
								write(clifd, output, output_len);
								ConnMsgLen = sprintf(ConnMsg, RED "Sending %d Bytes To %s:%d\n" RESET, output_len, inet_ntoa(client.sin_addr), client.sin_port);
								write(SOUT, ConnMsg, ConnMsgLen);
								j++;
							}
						}
						else {
							Conn_End_Label:
							ConnMsgLen = sprintf(ConnMsg, YEL "Ending connection with %s:%d\n" RESET, inet_ntoa(client.sin_addr), client.sin_port);
							write(SOUT, ConnMsg, ConnMsgLen);
							exit(0);
						}
					}if(cfds[1].revents == POLLIN){
						char servbuff[64];
						int serv_r = read(ConList[Curr_Con-1].pwfd[0], servbuff, sizeof(servbuff));
						if(!strncmp(servbuff, "list", 4)){	
							token = strtok(servbuff, " \n");	
							output_len = printlist(output);
							write(ConList[Curr_Con-1].prfd[1], output, output_len);
						}else 
							write(ConList[Curr_Con-1].cfd, servbuff, serv_r);
					}
				}
			}
		}
	}
}
