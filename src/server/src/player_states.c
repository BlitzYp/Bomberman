#define _DEFAULT_SOURCE
#include "../include/player_states.h"
// #include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

int send_move(server_t* server, int client_fd, uint8_t slot_id)
{
    // typedef struct {
    //     uint8_t id;
    //     uint8_t ready;
    //     char name[MAX_NAME_LEN];
    // } player_info;
    msg_moved_t moved;
    // uint8_t status;
    // char server_id[MAX_CLIENT_ID_LEN]="server";
    // player_info players[MAX_PLAYERS];
    player_t players[MAX_PLAYERS];
    uint8_t player_count=0;

    pthread_mutex_lock(&server->state.mutex);
    // status=(uint8_t)server->state.status;
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        players[player_count].id=slot->id;
        players[player_count].row=slot->p.row;
        players[player_count].col=slot->p.col;
        memcpy(players[player_count].name,slot->name,MAX_NAME_LEN);
        player_count++;

        // players[player_count].id=slot->id;
        // players[player_count].row=slot->p->row;
        // players[player_count].col=slot->p->col;
        // memcpy(players[player_count].name,slot->name,MAX_NAME_LEN);
        // player_count++;
    }
    pthread_mutex_unlock(&server->state.mutex);

    moved.header.msg_type=MSG_MOVED;
    moved.header.sender_id=slot_id; // this might need to be server? but maybe might be a player? 
    moved.header.target_id=slot_id; // is this a specific player?
    // moved.cell_index=make_cell_index();
    

    if (send_header(client_fd,&moved.header)<0) return -1;
    if (send_u8(client_fd,player_count)<0) return -1;

    for (uint8_t i=0;i<player_count;i++) {
        if (send_u8(client_fd,players[i].id)<0) return -1;
        if (send_u8(client_fd,players[i].row)<0) return -1;
        if (send_u8(client_fd,players[i].col)<0) return -1;
        // if (write_exact(client_fd, players[i].name,MAX_NAME_LEN)<0) return -1;
    }

    return 0;
}

int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header)
{
    msg_move_attempt_t msg;
    int x = server->state.players[slot_id].p.col, y = server->state.players[slot_id].p.row;
    int dir = -1;

    msg.header=header;
    // if (read_exact(client_fd,msg.client_id,MAX_CLIENT_ID_LEN)<0) return -1;
    // if (read_exact(client_fd,msg.player_name,MAX_NAME_LEN)<0) return -1;
    if (recv_u8(client_fd,&msg.direction)<0) return -1;
    dir = msg.direction;
    
    if (dir >= DIR_UP && dir <= DIR_RIGHT) {
        x = dir == DIR_LEFT ? x - 1 : dir == DIR_RIGHT ? x + 1 : x;
        y = dir == DIR_UP ? y - 1 : dir == DIR_DOWN ? y + 1 : y;
    }

    pthread_mutex_lock(&server->state.mutex);
    // TODO: Implement check if collision
    if (true) {
        server->state.players[slot_id].p.col = x;
        server->state.players[slot_id].p.row = y;
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (send_move(server,client_fd,slot_id)!=0) return -1;

    return 0;
}