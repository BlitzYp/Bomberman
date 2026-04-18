#define _DEFAULT_SOURCE
#include "../include/server.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

static int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header)
{
    msg_move_attempt_t msg;
    

    msg.header=header;

    pthread_mutex_lock(&server->state.mutex);
    // TODO: Implement check if collision
    if (true) {
        server->state.players[slot_id].p->row;
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (send_move(server,client_fd,slot_id)!=0) return -1;

    // printf("HELLO FROM SLOT %u\n",slot_id);
    return 0;
}