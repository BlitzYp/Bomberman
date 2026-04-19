#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

#include "game_state.h"
#include "../../shared/include/protocol.h"

typedef struct server {
     int listen_fd;
     int port;
     pthread_t tick_thread;
     game_state_t state;
} server_t;

int init_server(server_t* server, int port);
int run_server(server_t* server);
void shutdown_server(server_t* server);
int broadcast_header(server_t* server, msg_header_t* header);

#endif
