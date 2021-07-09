#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>
#include <netinet/tcp.h>
#include <signal.h>


struct _node {
	struct _node * next ;
	int socket_fd ;
} ;

typedef struct _node node;
typedef struct _node * node_ptr;
node socket_list = { 0x0, 0 } ;
int tot_socket = 0;

int turn = -1;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;







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

void *
find_prev (void * p){
        node_ptr itr;

	if (socket_list.next == p){
		return &socket_list;
	}

	for(itr = socket_list.next ; itr != 0x0 ; itr = itr -> next){
		if (itr -> next == p){
			return itr;
		}
	}
}

void *
unlink_node (node_ptr target)
{
	pthread_mutex_lock(&lock);

	close(target -> socket_fd);

	node_ptr tmp = find_prev(target);
	tmp -> next = target -> next;

	free(target);
	target = tmp;

	tot_socket = tot_socket - 1; 

	pthread_mutex_unlock(&lock);

	return target;
}


void
switch_turn (void)
{
	pthread_mutex_lock(&lock);

	/*
	if(tot_socket == 0){
		printf("No client connected\n");
		pthread_mutex_unlock(&lock);
		return ;
	}
	*/

	for(node_ptr itr = socket_list.next; itr != 0x0; itr = itr -> next){
		if(itr -> socket_fd == turn){
			if(itr -> next == 0x0){
				turn = socket_list.next -> socket_fd;
				break;
			}
			else{
				turn = itr -> next -> socket_fd;
				break;
			}
		}
	}
	printf("%d client view\n",turn);
	pthread_mutex_unlock(&lock);
}


void
handler (int sig)
{
	if (sig == SIGINT) {
		if(tot_socket == 0){
			exit(0);
		}
		else{
			switch_turn();
		}	
	}
}


//agent
void *
worker (void * fd)
{
	int conn = *(int *)fd;	
	printf("Client : %d is connected\n",conn);
		
	while(1){
		char buf[2048];
		int s = recv(conn, buf, 2048, 0);

		if(s == 0){
			goto err_discon;
		}


		buf[s] = 0x0;
		if(conn == turn){
			printf("%s\n",buf);
		}
	}





err_discon:
	pthread_mutex_lock(&lock);
	for(node_ptr itr = socket_list.next; ; itr - itr -> next){
		if(itr == 0x0){
			pthread_mutex_unlock(&lock);
			break;
		}

		if(itr -> socket_fd == conn){
			printf("%d client is disconnected\n", itr->socket_fd);
			close(itr -> socket_fd);
			node_ptr tmp = find_prev(itr);
			tmp -> next = itr -> next;
			free(itr);
			tot_socket = tot_socket - 1;
			
			
			if(tot_socket != 0 && turn == conn){
				turn = socket_list.next->socket_fd;
				printf("%d client view\n",turn);
			}
			
			pthread_mutex_unlock(&lock);
			break;	

		}
	}
	pthread_exit(0x0);
	exit(1);	
}

void *
shell ()
{

	while(1){
		if(socket_list.next == 0x0){
			continue;
		}
	
		char buf[1024];
		int i = 0;
		char c;
		while( (c = getc(stdin)) != '\n'){
			buf[i] = c;
			i++;
		}
		buf[i] = 0x0;
	

		pthread_mutex_lock(&lock);
		int conn = turn;
		pthread_mutex_unlock(&lock);

		int len = strlen(buf) ;
		send_s(sizeof(int), (char*)&len, conn);

		send_s(len, buf, conn);	
	}
}

int 
main(int argc, char const *argv[]) 
{
	signal(SIGINT, handler) ;

	int listen_fd; 
	struct sockaddr_in address; 
	int opt = 1; 
	int addrlen = sizeof(address); 

	char buffer[1024] = {0}; 

	listen_fd = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0 /*IP*/) ;
	if (listen_fd == 0)  { 
		perror("socket failed : "); 
		exit(EXIT_FAILURE); 
	}
	
	memset(&address, '0', sizeof(address)); 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY /* the localhost*/ ; 
	address.sin_port = htons(8723); 
	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : "); 
		exit(EXIT_FAILURE); 
	} 


	pthread_t main_thread;
	if(pthread_create(&main_thread, 0x0, shell, 0x0) < 0){
		perror("thread create error");
		exit(EXIT_FAILURE);
	}

	while (1) {
		if (listen(listen_fd, 16 /* the size of waiting queue*/) < 0) { 
			perror("listen failed : "); 
			exit(EXIT_FAILURE); 
		} 
		
		node_ptr tmp = malloc(sizeof(node));
		tmp -> next = 0x0;
		tmp -> socket_fd = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen);
		if(tmp -> socket_fd < 0){
			perror("accept failed ");
			exit(EXIT_FAILURE);
		}
		/*	
		int flag = 1;
		if (setsockopt(tmp -> socket_fd, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag)) == -1){
		  	perror("socket failed : ");
		  	exit(EXIT_FAILURE);
		}
		*/

		pthread_mutex_lock(&lock);

		tot_socket = tot_socket + 1;
		node_ptr last = find_prev(0x0);
		last -> next = tmp;

		if(tot_socket == 1){
			turn = tmp -> socket_fd;
		}

		pthread_mutex_unlock(&lock);

		pthread_t thread_t;
		if(pthread_create(&thread_t, 0x0, worker, (void*)&(tmp -> socket_fd)) < 0){
			perror("thread create eror");
			exit(EXIT_FAILURE);
		}
	}
}
