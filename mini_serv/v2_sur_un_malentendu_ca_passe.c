#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#define ERR_ARG "Wrong number of arguments\n"
#define ERR_FAT "Fatal error\n"

typedef struct s_client {
        int fd, id, new;
        struct s_client *next;
} t_client;

// Global server fd
int sockfd = -1;

// Static id counter for new client
static int g_id;

// all FD_SET needed for recv send and select
fd_set rd_set, wr_set, curr_set;

// Buffer user for "server: client %d just arrived", "server: client %d just left" and "client %d: " 
char server_buffer[64];

// Client linked list
t_client *clients = NULL;

// Exit in failure case
int fatal() {
        write(2, ERR_FAT, strlen(ERR_FAT));
        if (sockfd > -1)
                close(sockfd);
        exit(1);
}

// Get client by fd
t_client *get_client(int fd) {
        for (t_client *client = clients; client; client = client->next)
                if (client->fd == fd)
                        return (client);
        return (NULL);
}

// Get actual max fd
int get_max_fd() {
        int max = sockfd;

        for(t_client *client = clients; client; client = client->next)
                max = max > client->fd ? max : client->fd;
        return (max);
}

// Create client, add client to linked list and return his id
int new_client(int fd) {
        t_client *tmp = clients;
        t_client *new_client;

        if (!(new_client = calloc(1, sizeof(t_client))))
                fatal();

        new_client->fd = fd;
        new_client->id = g_id++;
        new_client->new = 1;

        if (!tmp)
                clients = new_client;
        else {
                while (tmp->next)
                        tmp = tmp->next;
                tmp->next = new_client;
        }
        return (new_client->id);
}

// Delete client and return his id
int del_client(int fd) {
        t_client *tmp = clients;
        t_client *del = clients;
        int id = get_client(fd)->id;

        if (clients->fd == fd) {
                clients = clients->next;
                free(del);
        } else {
                while (tmp->next->fd != fd)
                        tmp = tmp->next;
                del = tmp->next;
                tmp->next = tmp->next->next;
                free(del);
        }
        return (id);
}

// Send message to all clients
void send_all(char *buffer, size_t length, int sender) {
        for (t_client *client = clients; client; client = client->next)
                if (client->fd != sender && FD_ISSET(client->fd, &wr_set))
                        if (send(client->fd, buffer, length, 0) < 0)
                                fatal();
}

// Accept new client and add it
void accept_client() {
        int new_fd;

        if ((new_fd = accept(sockfd, NULL, NULL)) < 0)
                fatal();
        sprintf(server_buffer, "server: client %d just arrived\n", new_client(new_fd));
        send_all(server_buffer, strlen(server_buffer), new_fd);
        FD_SET(new_fd, &curr_set);
}

int main(int ac, char **av) {
        if (ac != 2) {
                write(2, ERR_ARG, strlen(ERR_ARG));
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

        FD_ZERO(&curr_set);
        FD_SET(sockfd, &curr_set);
        while (1) {
                wr_set = rd_set = curr_set;
                if (select(get_max_fd() + 1, &rd_set, &wr_set, NULL, NULL) < 0)
                        continue;
                for (int fd = 0; fd <= get_max_fd(); fd++) {
                        if (FD_ISSET(fd, &rd_set)) {
                                if (fd == sockfd) {
                                        accept_client();
                                        break;
                                } else {
                                        char c;
                                        if (recv(fd, &c, 1, 0) <= 0) {
                                                sprintf(server_buffer, "server: client %d just left\n", del_client(fd));
                                                send_all(server_buffer, strlen(server_buffer), fd);
                                                FD_CLR(fd, &curr_set);
                                                close(fd);
                                                break;
                                        } else {
                                                t_client *client = get_client(fd);
                                                if (client->new) {
                                                        sprintf(server_buffer, "client %d: ", client->id);
                                                        send_all(server_buffer, strlen(server_buffer), fd);
                                                }
                                                client->new = (c == '\n');
                                                send_all(&c, 1, fd);
                                        }
                                }
                        }
                }
        }
        return (0);
}
