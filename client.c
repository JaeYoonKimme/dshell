#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 
#include <netinet/tcp.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


pid_t worker;

int isworking = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

char command[2048];

void
send_s (size_t len, char * data, int sock)
{
	size_t s;
	while(len > 0 && (s = send(sock, data, len, 0)) > 0){
		data += s;
		len -= s;
	}
}

void
recv_s (size_t len, char * data, int sock)
{
	size_t s;
	while(len > 0 && (s = recv(sock, data, len, 0)) > 0){
		data += s;
		len -= s;
	}
}

char ** 
command_parser (char * command)
{
	char ** cmd_arr = (char **) malloc(sizeof(char*));
	
	char * tok = strtok(command," ");

	int i = 0;
	while(tok != 0x0){
		cmd_arr = (char**) realloc(cmd_arr, sizeof(char*) * (i+1));
		cmd_arr[i] = tok;
		i = i + 1;

		tok = strtok(NULL, " ");
	}
	return cmd_arr;
}


void *
command_reciever (void * fd)
{

	int conn = *(int *)fd;
	
	while(1){
		int size = -1;
		recv_s(sizeof(int), (char*)&size, conn);
		if(size == -1){
			printf("disconnected\n");
			close(conn);
			kill(worker,SIGKILL);
			exit(1);
		}

		recv_s(size, command, conn);
		command[size]=0x0;	
		
		if(strcmp(command,"$") == 0){
			kill(worker,SIGKILL);
		}

		pthread_mutex_lock(&lock);
		if(isworking == 1){
			continue;
		}
		else{
			pthread_cond_signal(&cond);
		}

		pthread_mutex_unlock(&lock);
	
	}	
}

void *
result_sender(void * fd)
{
	int conn = *(int *)fd;

	while(1){
		pthread_mutex_lock(&lock);
		pthread_cond_wait(&cond, &lock);
		isworking == 1;
		pthread_mutex_unlock(&lock);

		int pipes[2];
		if(pipe(pipes) == -1){
			perror("pipe error\n");
			exit(EXIT_FAILURE);
	       	}
		worker = fork();
		if (worker == 0) {
			close(pipes[0]);
			dup2(pipes[1], 1);
			dup2(pipes[1], 2);
			close(pipes[1]);
			
			char ** cmd_arr = command_parser(command);	
			execvp(cmd_arr[0],cmd_arr);
			//여기서 cmd_arr를 어떻게 free 시켜주지? 방법이없다.	
			printf("%s: command not found",cmd_arr[0]);
			exit(EXIT_FAILURE);
		}
	
		else if (worker > 0) {
			close(pipes[1]);
			char buf[2048];
			char * data = buf;
			int s;
			int len = 0;
			while( (s =read(pipes[0], data, 2048)) != 0){
				send_s(s,data,conn);
				data[s] = 0x0;
				printf("%s\n",data);
			}
	       	}
        	else /* worker < 0 */ {
			printf("Fork failed.\n") ;
		}

		pthread_mutex_lock(&lock);
		isworking = 0;
		pthread_mutex_unlock(&lock);
	}
}

int 
main(int argc, char const *argv[]) 
{ 
	struct sockaddr_in serv_addr; 
	int sock_fd ;
	int s, len ;
	char buffer[1024] = {0}; 
	char * data ;
	
	sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if (sock_fd <= 0) {
		perror("socket failed : ") ;
		exit(EXIT_FAILURE) ;
	} 
	int flag = 1;
	if (setsockopt(sock_fd, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag)) == -1){
		perror("socket failed : ");
		exit(EXIT_FAILURE);
	}

	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(8723); 
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		perror("inet_pton failed : ") ; 
		exit(EXIT_FAILURE) ;
	} 

	if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect failed : ") ;
		exit(EXIT_FAILURE) ;
	}


	pthread_t reciever, worker;
	if(pthread_create(&reciever, 0x0, command_reciever, (void*)&sock_fd) < 0){
		perror("thread_create error");
		exit(EXIT_FAILURE);
	}
	if(pthread_create(&worker, 0x0, result_sender, (void*)&sock_fd) < 0){
		perror("thread_create error");
		exit(EXIT_FAILURE);
	}

	pthread_join(reciever,0x0);
	pthread_join(worker,0x0);

	/*
	while(1){
		int size = -1;
		recv_s(sizeof(int), (char*)&size, sock_fd);
		if(size == -1){
			printf("disconnected\n");
			break;
		}

		char buf[1024];
		recv_s(size, buf, sock_fd);
		buf[size]=0x0;
		
		send_result(buf, sock_fd);
	}
	*/
}
 

