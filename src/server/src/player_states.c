#define _DEFAULT_SOURCE
#include "../include/player_states.h"
#include "../include/server.h"
#include "../../shared/include/net_utils.h"

#include <pthread.h>

int send_move(server_t* server, uint8_t slot_id)
{
    if (!server || slot_id>=MAX_PLAYERS) return -1;

    msg_moved_t moved;
    moved.player_id=slot_id;

    pthread_mutex_lock(&server->state.mutex);
    player_slot_t* slot=&server->state.players[slot_id];
    if (!slot->alive || !slot->connected || server->state.cols==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    uint16_t cell_index=make_cell_index(slot->p.row, slot->p.col, server->state.cols);

    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    moved.cell_index=cell_index;
    moved.header.msg_type=MSG_MOVED;
    moved.header.sender_id=slot_id;
    moved.header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&moved.header)<0) return -1;
        if (send_u8(client_fd[i],moved.player_id)<0) return -1;
        if (send_u16_be(client_fd[i],moved.cell_index)<0) return -1;
    }

    return 0;
}

int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header)
{
    msg_move_attempt_t msg;
    action_t action;
    int dir = -1;

    msg.header=header;
    if (recv_u8(client_fd,&msg.direction)<0) return -1;
    dir = msg.direction;
    if (dir<DIR_UP || dir>DIR_RIGHT) return -1;

    action.player_id=slot_id;
    action.type=ACTION_MOVE;
    action.direction=(uint8_t)dir;
    action.cell_index=0;

    pthread_mutex_lock(&server->state.mutex);
    if (enqueue_action(&server->state,action)!=0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    pthread_mutex_unlock(&server->state.mutex);

    return 0;
}
