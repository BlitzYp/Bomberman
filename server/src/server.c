/* TODO:
Write server loop and setup pthreads for players,
Add simple interaction with client with HELLO and movement
*/
#include "../include/server.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    server_t* server;
    int client_fd;
    uint8_t player_id;
} client_thread_args_t;

static int get_listen_socket(int port)
{
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if (fd<0) {
        perror("Error creating socket for listening...");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=port;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if (bind(fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
        perror("Error binding socket..");
        close(fd);
        return -1;
    }

    if (listen(fd,16)<0) {
        perror("Error listening to socket...");
        close(fd);
        return -1;
    }

    return fd;
}

int server_init(server_t* server,int port)
{
    memset(server,0,sizeof(*server));
    server->port=port;
    server->listen_fd=get_listen_socket(port);

    if (server->listen_fd<0) return -1;
    if (game_state_init(&server->state)!=0) {
        close(server->listen_fd);
        return -1;
    }
    return 0;
}

void shutdown_server(server_t* server)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++)
        if (server->state.players[i].socket_fd>=0)
            close(server->state.players[i].socket_fd);

    if (server->listen_fd>=0) close(server->listen_fd);
    game_state_destroy(&server->state);
}
