/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   copy.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gbaud <gbaud@student.42lyon.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/08/17 04:10:30 by gbaud             #+#    #+#             */
/*   Updated: 2021/08/17 05:59:45 by gbaud            ###   ########lyon.fr   */
/*                                                                            */
/* ************************************************************************** */

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ERR_ARG "Wrong number of arguments\n"
#define FAT_ERR "Fatal error\n"

typedef struct s_client {
    int id;
    int fd;
    struct s_client *next;
} t_client;

t_client *g_clients = NULL;

int sockfd = 0;
int g_id = 0;

fd_set rd_set;
fd_set wr_set;
fd_set cur_set;

char recv_buf_fail  [42];
char recv_buf       [42 * 4096];
char send_buf_before[42 * 4096];
char send_buf_after [42 * 4096 + 42];

void    fatal()
{
    write(2, FAT_ERR, strlen(FAT_ERR));
    close(sockfd);
    exit(1);
}

int     get_client_id(int fd)
{
    t_client *tmp = g_clients;

    while (tmp->next)
    {
        if (tmp->fd == fd)
            return (tmp->id);
        tmp = tmp->next;
    }
    return (-1);
}

int     get_max_client_fd()
{
    int         max = sockfd;
    t_client    *tmp = g_clients;

    while (tmp && tmp->next)
    {
        if (tmp->fd > max)
            max = tmp->fd;
        tmp = tmp->next;
    }
    return (max);
}

void    send_all(int sender, char *msg)
{
    t_client *tmp = g_clients;

    while (tmp->next)
    {
        if (tmp->fd != sender && FD_ISSET(tmp->fd, &wr_set))
        {
            if (send(tmp->fd, msg, strlen(msg), 0) < 0)
                fatal();
        }
        tmp = tmp->next;
    }
}

int     add_client(int fd)
{
    t_client *tmp;
    t_client *new_client;
    
    if (!(new_client = calloc(1, sizeof(t_client))))
        fatal();
    new_client->fd = fd;
    new_client->id = g_id++;
    new_client->next = NULL;
    if (!g_clients)
        g_clients = new_client;
    else
    {
        tmp = g_clients;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new_client;
    }
    return (new_client->id);
}

int    rm_client(int fd)
{
    int id = -1;
    t_client *tmp = g_clients;
    t_client *last = g_clients;
    
    if (tmp->fd == fd)
    {
        g_clients = g_clients->next;
        id = tmp->id;
        free(tmp);
        return (id);
    }
    while (tmp->next && tmp->fd != fd)
    {
        last = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
        return (id);
    last = tmp->next;
    id = tmp->id;
    free(tmp);
    return (id);
}

void    accept_client()
{
    int new_client;
    
    if ((new_client = accept(sockfd, NULL, NULL)) <= 0)
        fatal();
    sprintf(recv_buf_fail, "server: client %d just arrived\n", add_client(new_client));
    send_all(new_client, recv_buf_fail);
    FD_SET(new_client, &cur_set);
}

void    send_msgs(int fd)
{
    int i = 0;
    int j = 0;
    
    while (recv_buf[i])
    {
        send_buf_before[j++] = recv_buf[i];
        if (send_buf_before[i] == '\n')
        {
            sprintf(send_buf_after, "client %d: %s", get_client_id(fd), send_buf_before);
            send_all(fd, send_buf_after);
            bzero(&send_buf_before, strlen(send_buf_before));
            bzero(&send_buf_after, strlen(send_buf_after));
            j = 0;
        }
        i++;
    }
}

int     main(int ac, char **av)
{
    if (ac != 2)
    {
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
        
    FD_ZERO(&cur_set);
    FD_SET(sockfd, &cur_set);
    bzero(&recv_buf, sizeof(recv_buf));
    bzero(&send_buf_before, sizeof(send_buf_before));
    bzero(&send_buf_after, sizeof(send_buf_after));
    while (1)
    {
        wr_set = rd_set = cur_set;
        if (select(get_max_client_fd() + 1, &rd_set, &wr_set, NULL, NULL) < 0)
            continue;
        for (int i = 0; i <= get_max_client_fd(); i++)
            if (FD_ISSET(i, &rd_set))
            {
        printf("here0\n");
                if (i == sockfd)
                {
        printf("here1\n");
                    bzero(&recv_buf_fail, sizeof(recv_buf_fail));
                    accept_client();
        printf("ok\n");
                    break ;
                }
                else
                {
        printf("here2\n");
                    if (recv(i, recv_buf, sizeof(recv_buf), 0) <= 0)
                    {
        printf("here3\n");
                        bzero(&recv_buf_fail, sizeof(recv_buf_fail));
                        sprintf(recv_buf_fail, "server: client %d just left\n", rm_client(i));
                        send_all(i, recv_buf_fail);
                        FD_CLR(i, &cur_set);
                        close(i);
                        break ;
                    }
                    else
                        send_msgs(i);
                }
            }
    }
    return (0);
}