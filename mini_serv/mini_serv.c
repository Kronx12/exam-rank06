#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define FATAL_ERR "Fatal error\n"

typedef struct		s_client 
{
	int				fd;
    int             id;
	struct s_client	*next;
}	t_client;

t_client *g_clients = NULL;

int g_id = 0;
int sock_fd = 0;

fd_set curr_sock;
fd_set cpy_read;
fd_set cpy_write;

char msg[42];
char str[42 * 4096];
char tmp[42 * 4096];
char buf[42 * 4096 + 42];

// str -> buffer de reception

// tmp -> buffer de retour
// buf -> buffer de retour

// fatal() -> write(2), close server and exit(1) -> fatal_error
// get_id(fd) -> return client id with this fd
// get_max_fd() -> return max client fd
// send_all(fd, str) -> send data to all clients with other fd si ils sont dans le wr_set
// add_client_to_list(int fd) -> add client to linkedlist
// add_client() -> intercept accept and send to all the id
// rm_client() -> remove the client from linked_list
// ex_msg() -> prepare le msg a l'envoi
// main(ac, av) -> socket bind listen select (recv si <= 0 -> client just left)

void	fatal() 
{
	write(2, FATAL_ERR, strlen(FATAL_ERR));
	close(sock_fd);
	exit(1);
}

int     get_id(int fd)
{
    t_client *temp = g_clients;

    while (temp)
    {
        if (temp->fd == fd)
            return (temp->id);
        temp = temp->next;
    }
    return (-1);
}

int		get_max_fd() 
{
	int	max = sock_fd;
    t_client *temp = g_clients;

    while (temp)
    {
        if (temp->fd > max)
            max = temp->fd;
        temp = temp->next;
    }
    return (max);
}

void	send_all(int fd, char *str_req)
{
    t_client *temp = g_clients;

    while (temp)
    {
        if (temp->fd != fd && FD_ISSET(temp->fd, &cpy_write))
        {
            if (send(temp->fd, str_req, strlen(str_req), 0) < 0)
                fatal();
        }
        temp = temp->next;
    }
}

int		add_client_to_list(int fd)
{
    t_client *temp = g_clients;
    t_client *new;

    if (!(new = calloc(1, sizeof(t_client))))
        fatal();
    new->id = g_id++;
    new->fd = fd;
    new->next = NULL;
    if (!g_clients)
    {
        g_clients = new;
    }
    else
    {
        while (temp->next)
            temp = temp->next;
        temp->next = new;
    }
    return (new->id);
}

void    add_client()
{
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int client_fd;

    if ((client_fd = accept(sock_fd, (struct sockaddr *)&clientaddr, &len)) < 0)
        fatal();
    sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
    send_all(client_fd, msg);
    FD_SET(client_fd, &curr_sock);
}

int     rm_client(int fd)
{
    t_client *temp = g_clients;
    t_client *del;
    int id = get_id(fd);

    if (temp && temp->fd == fd)
    {
        g_clients = temp->next;
        free(temp);
    }
    else
    {
        while(temp && temp->next && temp->next->fd != fd)
            temp = temp->next;
        del = temp->next;
        temp->next = temp->next->next;
        free(del);
    }
    return (id);
}

void    ex_msg(int fd)
{
    int i = 0;
    int j = 0;

    while (str[i])
    {
        tmp[j] = str[i];
        j++;
        if (str[i] == '\n')
        {
            sprintf(buf, "client %d: %s", get_id(fd), tmp);
            send_all(fd, buf);
            j = 0;
            bzero(&tmp, strlen(tmp));
            bzero(&buf, strlen(buf));
        }
        i++;
    }
    bzero(&str, strlen(str));
}

int     main(int ac, char **av)
{
    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    struct sockaddr_in servaddr;
    uint16_t port = atoi(av[1]);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(port);

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal();
    if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        fatal();
    if (listen(sock_fd, 0) < 0)
        fatal();
    
    FD_ZERO(&curr_sock);
    FD_SET(sock_fd, &curr_sock);
    bzero(&tmp, sizeof(tmp));
    bzero(&buf, sizeof(buf));
    bzero(&str, sizeof(str));
    while(1)
    {
        cpy_write = cpy_read = curr_sock;
        if (select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
            continue;
        for (int fd = 0; fd <= get_max_fd(); fd++)
        {
            if (FD_ISSET(fd, &cpy_read))
            {
        printf("here0\n");
                if (fd == sock_fd)
                {
        printf("here1\n");
                    bzero(&msg, sizeof(msg));
                    add_client();
        printf("ok\n");
                    break;
                }
                else
                {
        printf("here2\n");
                    if (recv(fd, str, sizeof(str), 0) <= 0)
                    {
        printf("here3\n");
                        bzero(&msg, sizeof(msg));
                        sprintf(msg, "server: client %d just left\n", rm_client(fd));
                        send_all(fd, msg);
                        FD_CLR(fd, &curr_sock);
                        close(fd);
                        break;
                    }
                    else
                    {
                        ex_msg(fd);
                    }
                }
            }
            
        }
        
    }
    return (0);
}