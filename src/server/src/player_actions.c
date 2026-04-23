#define _DEFAULT_SOURCE
#include "../include/player_actions.h"
#include "../include/server.h"
#include "../../shared/include/map_loader.h"
#include "../../shared/include/net_utils.h"

#include <pthread.h>

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

void handle_action_bomb(server_t* server,bomb_t* bomb,action_t action)
{
    // int x=bomb->col,y=bomb->row;
    int x=0,y=0;
    y=action.cell_index/server->state.map.cols;
    x=action.cell_index%server->state.map.cols;

    if (y>=0 && x>=0 && y<server->state.map.rows && x<server->state.map.cols) {
        // Check collision with other players
        if (map_is_walkable(&server->state.map,(uint16_t)y,(uint16_t)x)) {
            server->state.map.tiles[action.cell_index]=TILE_BOMB;
            bomb->state=BOMB_PLANTED;
            bomb->col=x;
            bomb->row=y;
            bomb->owner_id=action.player_id;
            bomb->radius=server->state.players[action.player_id].p.bomb_radius;
            bomb->timer_ticks=server->state.players[action.player_id].p.bomb_timer_ticks;
            bomb->explosion_ticks=0;
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
