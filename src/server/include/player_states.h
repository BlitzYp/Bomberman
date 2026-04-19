#ifndef PLAYER_STATES_H
#define PLAYER_STATES_H

#include <pthread.h>

// #include "server.h"
#include "game_state.h"
#include "../../shared/include/net_utils.h"

typedef struct {
    int listen_fd;
    int port;
    pthread_t tick_thread;
    game_state_t state;
} server_t;

int send_move(server_t* server, int client_fd, uint8_t slot_id);
int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header);

#endif
