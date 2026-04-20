#define _DEFAULT_SOURCE
#include "../include/player_states.h"
#include "../include/server.h"
#include "../../shared/include/map_loader.h"
#include "../../shared/include/net_utils.h"

#include <pthread.h>
#include <stdio.h>

int send_move(server_t* server, uint8_t slot_id)
{
    if (!server || slot_id>=MAX_PLAYERS) return -1;

    msg_moved_t moved;
    moved.player_id=slot_id;

    pthread_mutex_lock(&server->state.mutex);
    player_slot_t* slot=&server->state.players[slot_id];
    if (!slot->alive || !slot->connected || server->state.map.cols==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    uint16_t cell_index=make_cell_index(slot->p.row, slot->p.col, server->state.map.cols);

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

void send_move_broadcast(server_t* server, bool* moved_players)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!moved_players[i]) continue;
        if (send_move(server,i)!=0) {
            // log err
            printf("Error sending movement data to the client %s with id %d",server->state.players[i].name,server->state.players[i].id);
        }
    }
}

void handle_action_move(server_t* server,player_slot_t* slot,action_t action)
{
    int x=slot->p.col,y=slot->p.row;
    switch (action.direction) {
        case DIR_LEFT:
            x--;
            break;
        case DIR_RIGHT:
            x++;
            break;
        case DIR_UP:
            y--;
            break;
        case DIR_DOWN:
            y++;
            break;
        default: break;
    }

    if (y>=0 && x>=0 && y<server->state.map.rows && x<server->state.map.cols) {
        // Check collision with other players
        bool valid=true;
        for (uint8_t i=0;i<MAX_PLAYERS;i++) {
            player_slot_t* p_slot=&server->state.players[i];
            if (!p_slot->connected || !p_slot->alive || p_slot->id==slot->id) continue;
            if (p_slot->p.row==y && p_slot->p.col==x) {
                valid=false;
                break;
            }
        }
        if (map_is_walkable(&server->state.map,(uint16_t)y,(uint16_t)x) && valid) {
            slot->p.row=y;
            slot->p.col=x;
        }
    }
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

int send_bomb(server_t* server, uint8_t slot_id)
{
    if (!server || slot_id>=MAX_PLAYERS) return -1;

    msg_bomb_t bomb;
    bomb.player_id=slot_id;

    pthread_mutex_lock(&server->state.mutex);
    player_slot_t* slot=&server->state.players[slot_id];
    if (!slot->alive || !slot->connected || server->state.map.cols==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    slot->p.bomb_count--;
    uint16_t cell_index=make_cell_index(slot->p.row, slot->p.col, server->state.map.cols);

    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    bomb.cell_index=cell_index;
    bomb.header.msg_type=MSG_BOMB;
    bomb.header.sender_id=slot_id;
    bomb.header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&bomb.header)<0) return -1;
        if (send_u8(client_fd[i],bomb.player_id)<0) return -1;
        if (send_u16_be(client_fd[i],bomb.cell_index)<0) return -1;
    }

    return 0;
}

void send_bomb_broadcast(server_t* server, bool* bomb_placed_players)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!bomb_placed_players[i]) continue;
        if (send_bomb(server,i)!=0) {
            // log err
            printf("Error sending bomb data to the client %s with id %d",server->state.players[i].name,server->state.players[i].id);
        }
    }
}

void handle_action_bomb(server_t* server,bomb_t* slot,action_t action)
{
    // int x=slot->col,y=slot->row;
    int x=0,y=0;
    y=action.cell_index/server->state.map.cols;
    x=action.cell_index%server->state.map.cols;

    if (y>=0 && x>=0 && y<server->state.map.rows && x<server->state.map.cols) {
        // Check collision with other players
        if (map_is_walkable(&server->state.map,(uint16_t)y,(uint16_t)x)) {
            slot->active=1;
            slot->col=x;
            slot->row=y;
            slot->owner_id=action.player_id;
        }
    }
}

int handle_bomb(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header)
{
    // msg_bomb_attempt_t msg;
    action_t action;
    int dir = -1;
    int col=0,row=0;

    // msg.header=header;

    // if (server->state.players[slot_id].p.bomb_count<1) return -1;

    col=server->state.players[slot_id].p.col;
    row=server->state.players[slot_id].p.row;
    uint16_t cell_index=make_cell_index(row, col, server->state.map.cols);

    action.player_id=slot_id;
    action.type=ACTION_BOMB;
    action.direction=dir;
    action.cell_index=cell_index;

    pthread_mutex_lock(&server->state.mutex);
    if (enqueue_action(&server->state,action)!=0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    pthread_mutex_unlock(&server->state.mutex);

    return 0;
}

int bomb_explode_start(server_t* server,bomb_t* slot)
{
    msg_header_t header;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p_slot=&server->state.players[i];
        if (!p_slot->connected) continue;
        client_fd[client_count++]=p_slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    header.msg_type=MSG_EXPLOSION_START;
    header.sender_id=slot->owner_id;
    header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&header)<0) return -1;
    }

    return 0;
}