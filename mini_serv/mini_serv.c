#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>

#define ERR_FATAL "Fatal eror\n"
#define ERR_ARGS "Wrong number of arguments\n"
#define BUFFER_SIZE 60000000

typedef struct s_client {
	int id;
	int fd;
	struct s_client *next;
} t_client;

int g_id = 0;
int sockfd;

fd_set wr_set, rd_set, cur_set;

char buff_err[42];
static char buff[BUFFER_SIZE];

t_client *clients = NULL;

void fatal() {
	write(2, ERR_FATAL, strlen(ERR_FATAL));
	close(sockfd);
	exit(1);
}

int get_max_fd() {
	int max = sockfd;

	for (t_client *c = clients; c; c = c->next)
		max = max > c->fd ? max : c->fd;
	return (max);
}

int get_client_id(int fd) {
	for (t_client *c = clients; c; c = c->next)
		if (c->fd == fd)
			return c->id;
	return -1;
}

int add_client(int fd) {
	t_client *new_client;
	t_client *tmp = clients;

	if (!(new_client = calloc(sizeof(t_client), 1)))
		fatal();
	new_client->id = g_id++;
	new_client->fd = fd;
	if (!clients)
		clients = new_client;
	else {
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new_client;
	}
	return (new_client->id);
}

void send_all(char *buff, int len, int sender) {
	for (t_client *c = clients;c;c = c->next)
		if (c->fd != sender && FD_ISSET(c->fd, &wr_set))
			if (send(c->fd, buff, len, 0) < 0)
				fatal();
}

void accept_client() {
	int new_fd;

	if ((new_fd = accept(sockfd, NULL, NULL)) < 0)
		fatal();
	sprintf(buff_err, "server: client %d just arrived\n", add_client(new_fd));
	send_all(buff_err, strlen(buff_err), new_fd);
	FD_SET(new_fd, &cur_set);
}

int rm_client(int fd) {
	t_client *tmp = clients;
	t_client *prev;
	t_client *del;
	int id = get_client_id(fd);
	
	if (tmp->fd == fd) {
		clients = clients->next;
		free(tmp);
	} else {
		while (tmp->fd != fd) {
			prev = tmp;
			tmp = tmp->next;
		}
		del = tmp;
		prev->next = tmp->next;
		free(del);
	}
	return id;
}

int nbrlen(int nb) {
	int res = 1;

	while (nb /= 10)
		res++;
	return (res);
}

void send_msg(int sender) {
	char *start = &buff[32];
	char *end = buff;
	int len = 9 + nbrlen(sender);

	while (start != end) {
		char tmp = *start;
		sprintf(start - len, "client %d: ", sender);
		start[0] = tmp;
		end = strstr(start, "\n") + 1;
		if (end == NULL)
			end = start + strlen(start);
		send_all(start - len, len + (end - start), sender);
		start = end;
	}
}

int main(int ac, char **av) {
	if (ac != 2) {
		write(2, ERR_ARGS, strlen(ERR_ARGS));
		exit(1);
	}

	struct sockaddr_in servaddr;
    	uint16_t port = atoi(av[1]);
    	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(port);
    
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	        fatal();
	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        	fatal();
	if (listen(sockfd, 0) < 0)
        	fatal();
        
	FD_ZERO(&cur_set);
    	FD_SET(sockfd, &cur_set);
    	while (1)
   	{
        	wr_set = rd_set = cur_set;
        	if (select(get_max_fd() + 1, &rd_set, &wr_set, NULL, NULL) < 0)
            		continue;
        	for (int i = 0; i <= get_max_fd(); i++)
	            	if (FD_ISSET(i, &rd_set))
        	    	{
                		if (i == sockfd)
                		{
                			bzero(&buff_err, sizeof(buff_err));
	                    		accept_client();
        	            		break ;
                		}
                		else
	               		{
        	            		if (recv(i, &buff[32], BUFFER_SIZE - 32, 0) <= 0)
                	    		{
		        	                bzero(&buff_err, sizeof(buff_err));
                			        sprintf(buff_err, "server: client %d just left\n", rm_client(i));
	        	                	send_all(buff_err, strlen(buff_err), i);
	        	        	        FD_CLR(i, &cur_set);
			                        close(i);
                			        break ;
					}
		                	else
                		        	send_msg(i);
                		}
        		}
    	}
	return (0);
}
