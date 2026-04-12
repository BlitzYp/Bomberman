#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

#include "game_state.h"

typedef struct {
    int listen_fd;
    int port;
    pthread_t tick_thread;
    game_state_t state;
} server_t;

int server_init(server_t* server, int port);
int server_run(server_t* server);
void shutdown_server(server_t* server);

#endif
