#define _DEFAULT_SOURCE
#include "../include/server_messages.h"
#include "../../shared/include/net_utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int send_status(int client_fd,uint8_t target_id,game_status_t status)
{
    msg_set_status_t state;

    if (client_fd<0) return -1;

    state.header.msg_type=MSG_SET_STATUS;
    state.header.sender_id=SERVER_TARGET_ID;
    state.header.target_id=target_id;
    state.status=(uint8_t)status;

    if (send_header(client_fd,&state.header)<0) return -1;
    if (send_u8(client_fd,state.status)<0) return -1;

    return 0;
}

static int send_move_to_client(server_t* server,int client_fd,uint8_t target_id,uint8_t slot_id)
{
    msg_moved_t moved;

    if (!server || client_fd<0 || slot_id>=MAX_PLAYERS) return -1;

    pthread_mutex_lock(&server->state.mutex);
    player_slot_t* slot=&server->state.players[slot_id];
    if (!slot->alive || !slot->connected || server->state.map.cols==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    uint16_t cell_index=make_cell_index(slot->p.row,slot->p.col,server->state.map.cols);
    pthread_mutex_unlock(&server->state.mutex);

    moved.header.msg_type=MSG_MOVED;
    moved.header.sender_id=slot_id;
    moved.header.target_id=target_id;
    moved.player_id=slot_id;
    moved.cell_index=cell_index;

    if (send_header(client_fd,&moved.header)<0) return -1;
    if (send_u8(client_fd,moved.player_id)<0) return -1;
    if (send_u16_be(client_fd,moved.cell_index)<0) return -1;

    return 0;
}

static int send_winner_to_client(int client_fd,uint8_t target_id,uint8_t player_id)
{
    msg_winner_t winner;

    if (client_fd<0 || player_id>=MAX_PLAYERS) return -1;

    winner.header.msg_type=MSG_WINNER;
    winner.header.sender_id=player_id;
    winner.header.target_id=target_id;
    winner.player_id=player_id;

    if (send_header(client_fd,&winner.header)<0) return -1;
    if (send_u8(client_fd,winner.player_id)<0) return -1;

    return 0;
}

int send_welcome(server_t* server, int client_fd, uint8_t slot_id)
{
    typedef struct {
        uint8_t id;
        uint8_t ready;
        char name[MAX_NAME_LEN];
    } player_info;

    msg_header_t header;
    uint8_t status;
    char server_id[MAX_CLIENT_ID_LEN]="server";
    player_info players[MAX_PLAYERS];
    uint8_t player_count=0;

    pthread_mutex_lock(&server->state.mutex);
    status=(uint8_t)server->state.status;
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        players[player_count].id=slot->id;
        players[player_count].ready=slot->ready ? 1 : 0;
        memcpy(players[player_count].name,slot->name,MAX_NAME_LEN);
        player_count++;
    }
    pthread_mutex_unlock(&server->state.mutex);

    header.msg_type=MSG_WELCOME;
    header.sender_id=slot_id;
    header.target_id=slot_id;

    if (send_header(client_fd,&header)<0) return -1;
    if (write_exact(client_fd, server_id, MAX_CLIENT_ID_LEN)<0) return -1;
    if (send_u8(client_fd,status)<0) return -1;
    if (send_u8(client_fd,player_count)<0) return -1;

    for (uint8_t i=0;i<player_count;i++) {
        if (send_u8(client_fd,players[i].id)<0) return -1;
        if (send_u8(client_fd,players[i].ready)<0) return -1;
        if (write_exact(client_fd, players[i].name,MAX_NAME_LEN)<0) return -1;
    }

    return 0;
}

int send_disconnect(int client_fd, uint8_t target_id)
{
    msg_header_t header;

    header.msg_type=MSG_DISCONNECT;
    header.sender_id=SERVER_TARGET_ID;
    header.target_id=target_id;

    return send_header(client_fd,&header);
}

int send_leave_broadcast(server_t* server, uint8_t leaving_player_id)
{
    if (!server || leaving_player_id>=MAX_PLAYERS) return -1;

    int client_fds[MAX_PLAYERS];
    uint8_t client_count=0;

    pthread_mutex_lock(&server->state.mutex);
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* player=&server->state.players[i];
        if (!player->connected || player->id==leaving_player_id) continue;
        client_fds[client_count++]=player->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    msg_header_t header;
    header.msg_type=MSG_LEAVE;
    header.sender_id=leaving_player_id;
    header.target_id=BROADCAST_TARGET_ID;

    int result=0;
    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fds[i],&header)<0) result=-1;
    }

    return result;
}

int send_hello_broadcast(server_t* server, uint8_t joined_player_id, const char* client_id, const char* player_name)
{
    msg_hello_t msg;
    int client_fds[MAX_PLAYERS];
    uint8_t client_count=0;

    if (!server || joined_player_id>=MAX_PLAYERS || !client_id || !player_name) return -1;

    pthread_mutex_lock(&server->state.mutex);
    if (!server->state.players[joined_player_id].connected) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* player=&server->state.players[i];
        if (!player->connected || i==joined_player_id) continue;
        client_fds[client_count++]=player->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    memset(&msg,0,sizeof(msg));
    msg.header.msg_type=MSG_HELLO;
    msg.header.sender_id=joined_player_id;
    msg.header.target_id=BROADCAST_TARGET_ID;
    memcpy(msg.client_id,client_id,MAX_CLIENT_ID_LEN);
    memcpy(msg.player_name,player_name,MAX_NAME_LEN);

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fds[i],&msg.header)<0) return -1;
        if (write_exact(client_fds[i],msg.client_id,MAX_CLIENT_ID_LEN)<0) return -1;
        if (write_exact(client_fds[i],msg.player_name,MAX_NAME_LEN)<0) return -1;
    }

    return 0;
}

int send_map(int client_fd, uint8_t target_id, const map_t* map)
{
    msg_header_t header;

    if (!map || !map->tiles) return -1;

    header.msg_type=MSG_MAP;
    header.sender_id=SERVER_TARGET_ID;
    header.target_id=target_id;

    if (send_header(client_fd,&header)<0) return -1;
    if (send_u16_be(client_fd,map->rows)<0) return -1;
    if (send_u16_be(client_fd,map->cols)<0) return -1;

    for (uint32_t i=0;i<(uint32_t)map->rows*map->cols;i++) {
        if (send_u8(client_fd,(uint8_t)map->tiles[i])<0) return -1;
    }

    return 0;
}

int broadcast_map(server_t* server)
{
    if (!server) return -1;
    int client_fds[MAX_PLAYERS];
    uint8_t client_ids[MAX_PLAYERS],client_count=0;

    pthread_mutex_lock(&server->state.mutex);
    uint16_t rows=server->state.map.rows,cols=server->state.map.cols;
    tile_t* tiles=malloc(rows*cols*sizeof(*tiles));

    if (!tiles) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }

    memcpy(tiles,server->state.map.tiles,rows*cols*sizeof(*tiles));

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p=&server->state.players[i];
        if (!p->connected) continue;
        client_fds[client_count]=p->socket_fd;
        client_ids[client_count++]=p->id;
    }

    pthread_mutex_unlock(&server->state.mutex);

    map_t curr_map;
    memset(&curr_map,0,sizeof(curr_map));
    curr_map.rows=rows;
    curr_map.cols=cols;
    curr_map.tiles=tiles;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_map(client_fds[i],client_ids[i],&curr_map)<0) {
            free(tiles);
            return -1;
        }
    }

    free(tiles);
    return 0;
}

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
    uint16_t cell_index=make_cell_index(slot->p.row,slot->p.col,server->state.map.cols);

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
            printf("Error sending movement data to the client %s with id %d",server->state.players[i].name,server->state.players[i].id);
        }
    }
}

int send_bomb(server_t* server, bomb_t* bomb_state)
{
    if (!server || !bomb_state || bomb_state->owner_id>=MAX_PLAYERS) return -1;

    msg_bomb_t bomb;
    bomb.player_id=bomb_state->owner_id;

    pthread_mutex_lock(&server->state.mutex);
    if (server->state.map.cols==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    uint16_t cell_index=make_cell_index(bomb_state->row,bomb_state->col,server->state.map.cols);

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
    bomb.header.sender_id=bomb_state->owner_id;
    bomb.header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&bomb.header)<0) return -1;
        if (send_u8(client_fd[i],bomb.player_id)<0) return -1;
        if (send_u16_be(client_fd[i],bomb.cell_index)<0) return -1;
    }

    return 0;
}

void send_bomb_broadcast(server_t* server, bool* placed_bombs)
{
    for (uint8_t i=0;i<MAX_BOMBS;i++) {
        if (!placed_bombs[i]) continue;
        if (send_bomb(server,&server->state.bombs[i])!=0) {
            printf("Error sending bomb data for bomb slot %u\n",i);
        }
    }
}

void send_exploding_broadcast(server_t* server, bool* started_bombs)
{
    for (uint8_t i=0;i<MAX_BOMBS;i++) {
        if (!started_bombs[i]) continue;
        if (send_explode_start(server,&server->state.bombs[i])!=0) {
            printf("Error sending explode start data for bomb slot %u\n",i);
        }
    }
}

void send_end_explode_broadcast(server_t* server, bool* ended_bombs)
{
    for (uint8_t i=0;i<MAX_BOMBS;i++) {
        if (!ended_bombs[i]) continue;
        if (send_explode_end(server,&server->state.bombs[i])!=0) {
            printf("Error sending explode end data for bomb slot %u\n",i);
        }
    }
}

int send_death(server_t* server, uint8_t player_id)
{
    if (!server || player_id>=MAX_PLAYERS) return -1;

    msg_death_t death;
    death.player_id=player_id;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    death.header.msg_type=MSG_DEATH;
    death.header.sender_id=player_id;
    death.header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&death.header)<0) return -1;
        if (send_u8(client_fd[i],death.player_id)<0) return -1;
    }

    return 0;
}

void send_death_broadcast(server_t* server, bool* dead_players)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!dead_players[i]) continue;
        if (send_death(server,i)!=0) {
            printf("Error sending death data to the client %s with id %d",server->state.players[i].name,server->state.players[i].id);
        }
    }
}

int send_explode_start(server_t* server, bomb_t* bomb)
{
    msg_explosion_t explosion;

    pthread_mutex_lock(&server->state.mutex);
    uint16_t cell_index=make_cell_index(bomb->row,bomb->col,server->state.map.cols);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p_slot=&server->state.players[i];
        if (!p_slot->connected) continue;
        client_fd[client_count++]=p_slot->socket_fd;
    }

    pthread_mutex_unlock(&server->state.mutex);

    explosion.header.msg_type=MSG_EXPLOSION_START;
    explosion.header.sender_id=bomb->owner_id;
    explosion.header.target_id=BROADCAST_TARGET_ID;
    explosion.cell_index=cell_index;
    explosion.radius=bomb->radius;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&explosion.header)<0) return -1;
        if (send_u8(client_fd[i],explosion.radius)<0) return -1;
        if (send_u16_be(client_fd[i],explosion.cell_index)<0) return -1;
    }

    return 0;
}

int send_explode_end(server_t* server, bomb_t* bomb)
{
    msg_explosion_t explosion;

    pthread_mutex_lock(&server->state.mutex);
    uint16_t cell_index=make_cell_index(bomb->row,bomb->col,server->state.map.cols);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p_slot=&server->state.players[i];
        if (!p_slot->connected) continue;
        client_fd[client_count++]=p_slot->socket_fd;
    }

    pthread_mutex_unlock(&server->state.mutex);

    explosion.header.msg_type=MSG_EXPLOSION_END;
    explosion.header.sender_id=bomb->owner_id;
    explosion.header.target_id=BROADCAST_TARGET_ID;
    explosion.cell_index=cell_index;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&explosion.header)<0) return -1;
        if (send_u16_be(client_fd[i],explosion.cell_index)<0) return -1;
    }

    return 0;
}

int send_winner_broadcast(server_t* server, uint8_t player_id)
{
    if (!server || player_id>=MAX_PLAYERS) return -1;

    msg_winner_t winner;
    winner.player_id=player_id;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    winner.header.msg_type=MSG_WINNER;
    winner.header.sender_id=player_id;
    winner.header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&winner.header)<0) return -1;
        if (send_u8(client_fd[i],winner.player_id)<0) return -1;
    }

    return 0;
}


int send_status_broadcast(server_t* server,game_status_t status)
{
    if (!server) return -1;

    msg_set_status_t state;
    state.status=status;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    state.header.msg_type=MSG_SET_STATUS;
    state.header.sender_id=SERVER_TARGET_ID;
    state.header.target_id=BROADCAST_TARGET_ID;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&state.header)<0) return -1;
        if (send_u8(client_fd[i],state.status)<0) return -1;
    }

    return 0;
}

int send_selected_map(int client_fd,uint8_t target_id,const char* map_name)
{
    msg_selected_map_t msg;

    if (client_fd<0 || !map_name) return -1;

    memset(&msg,0,sizeof(msg));
    msg.header.msg_type=MSG_SELECTED_MAP;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=target_id;
    snprintf(msg.map_name,sizeof(msg.map_name),"%s",map_name);

    if (send_header(client_fd,&msg.header)<0) return -1;
    if (write_exact(client_fd,msg.map_name,MAX_MAP_NAME_LEN)<0) return -1;

    return 0;
}

int broadcast_selected_map(server_t* server)
{
    int client_fd[MAX_PLAYERS];
    uint8_t client_ids[MAX_PLAYERS];
    uint8_t client_count=0;
    char map_name[MAX_MAP_NAME_LEN];

    if (!server) return -1;

    pthread_mutex_lock(&server->state.mutex);
    snprintf(map_name,sizeof(map_name),"%s",game_state_selected_map_name(&server->state));
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count]=slot->socket_fd;
        client_ids[client_count++]=slot->id;
    }
    pthread_mutex_unlock(&server->state.mutex);

    for (uint8_t i=0;i<client_count;i++) {
        if (send_selected_map(client_fd[i],client_ids[i],map_name)<0) return -1;
    }

    return 0;
}

int send_full_sync(server_t* server,int client_fd,uint8_t target_id)
{
    bool synced_players[MAX_PLAYERS]={0};
    uint8_t winner_id=SERVER_TARGET_ID;
    bool has_winner=false;
    char map_name[MAX_MAP_NAME_LEN];
    map_t map_snapshot;

    if (!server || client_fd<0 || target_id>=MAX_PLAYERS) return -1;

    pthread_mutex_lock(&server->state.mutex);
    game_status_t status=server->state.status;
    if (!server->state.map.tiles) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    uint16_t rows=server->state.map.rows;
    uint16_t cols=server->state.map.cols;
    snprintf(map_name,sizeof(map_name),"%s",game_state_selected_map_name(&server->state));
    tile_t* tiles=malloc((size_t)rows*cols*sizeof(*tiles));
    if (!tiles) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    memcpy(tiles,server->state.map.tiles,(size_t)rows*cols*sizeof(*tiles));
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* player=&server->state.players[i];
        if (!player->connected || !player->alive) continue;
        synced_players[i]=true;
        winner_id=i;
        has_winner=true;
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (send_status(client_fd,target_id,status)<0) {
        free(tiles);
        return -1;
    }
    if (send_selected_map(client_fd,target_id,map_name)<0) {
        free(tiles);
        return -1;
    }
    memset(&map_snapshot,0,sizeof(map_snapshot));
    map_snapshot.rows=rows;
    map_snapshot.cols=cols;
    map_snapshot.tiles=tiles;
    if (send_map(client_fd,target_id,&map_snapshot)<0) {
        free(tiles);
        return -1;
    }
    free(tiles);
    if (send_all_bonuses(server,client_fd,target_id)<0) return -1;

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!synced_players[i]) continue;
        if (send_move_to_client(server,client_fd,target_id,i)<0) return -1;
    }

    if (status==GAME_END && has_winner) {
        if (send_winner_to_client(client_fd,target_id,winner_id)<0) return -1;
    }

    return 0;
}


int send_bonus_available(server_t* server,uint16_t cell_index,bonus_type_t bonus)
{
    if (!server || bonus==BONUS_NONE) return -1;

    msg_bonus_available_t msg;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    msg.header.msg_type=MSG_BONUS_AVAILABLE;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=BROADCAST_TARGET_ID;

    msg.bonus_type=(uint8_t)bonus;
    msg.cell_index=cell_index;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&msg.header)<0) return -1;
        if (send_u8(client_fd[i],msg.bonus_type)<0) return -1;
        if (send_u16_be(client_fd[i],msg.cell_index)<0) return -1;
    }

    return 0;
}

int send_bonus_retrieved(server_t* server,uint8_t player_id,uint16_t cell_index)
{
    if (!server || player_id>=MAX_PLAYERS) return -1;

    msg_bonus_retrieved_t msg;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    msg.header.msg_type=MSG_BONUS_RETRIEVED;
    msg.header.sender_id=player_id;
    msg.header.target_id=BROADCAST_TARGET_ID;

    msg.cell_index=cell_index;
    msg.player_id=player_id;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&msg.header)<0) return -1;
        if (send_u8(client_fd[i],msg.player_id)<0) return -1;
        if (send_u16_be(client_fd[i],msg.cell_index)<0) return -1;
    }

    return 0;
}

int send_block_destroyed(server_t* server,uint16_t cell_index)
{
    if (!server) return -1;

    msg_block_destroyed_t msg;

    pthread_mutex_lock(&server->state.mutex);
    uint8_t client_count=0;
    int client_fd[MAX_PLAYERS];
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count++]=slot->socket_fd;
    }
    pthread_mutex_unlock(&server->state.mutex);

    msg.header.msg_type=MSG_BLOCK_DESTROYED;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=BROADCAST_TARGET_ID;
    msg.cell_index=cell_index;

    for (uint8_t i=0;i<client_count;i++) {
        if (send_header(client_fd[i],&msg.header)<0) return -1;
        if (send_u16_be(client_fd[i],msg.cell_index)<0) return -1;
    }

    return 0;
}

int send_all_bonuses(server_t* server,int client_fd,uint8_t target_id)
{
    msg_bonus_available_t msg;

    if (!server || client_fd<0) return -1;

    pthread_mutex_lock(&server->state.mutex);
    uint16_t rows=server->state.map.rows;
    uint16_t cols=server->state.map.cols;
    uint32_t cell_count=(uint32_t)rows*cols;
    bonus_type_t* bonuses=malloc(cell_count*sizeof(*bonuses));
    if (!bonuses) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    memcpy(bonuses,server->state.bonuses,cell_count*sizeof(*bonuses));
    pthread_mutex_unlock(&server->state.mutex);

    msg.header.msg_type=MSG_BONUS_AVAILABLE;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=target_id;

    for (uint32_t i=0;i<cell_count;i++) {
        if (bonuses[i]==BONUS_NONE) continue;
        msg.bonus_type=(uint8_t)bonuses[i];
        msg.cell_index=(uint16_t)i;
        if (send_header(client_fd,&msg.header)<0
            || send_u8(client_fd,msg.bonus_type)<0
            || send_u16_be(client_fd,msg.cell_index)<0) {
            free(bonuses);
            return -1;
        }
    }

    free(bonuses);
    return 0;
}

int broadcast_all_bonuses(server_t* server)
{
    int client_fd[MAX_PLAYERS];
    uint8_t client_ids[MAX_PLAYERS];
    uint8_t client_count=0;

    if (!server) return -1;

    pthread_mutex_lock(&server->state.mutex);
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* slot=&server->state.players[i];
        if (!slot->connected) continue;
        client_fd[client_count]=slot->socket_fd;
        client_ids[client_count++]=slot->id;
    }
    pthread_mutex_unlock(&server->state.mutex);

    for (uint8_t i=0;i<client_count;i++) {
        if (send_all_bonuses(server,client_fd[i],client_ids[i])!=0) return -1;
    }

    return 0;
}

void send_bonus_available_broadcast(server_t *server, bool* bonus_cells_changed,bonus_type_t* types)
{
    if (!server || !bonus_cells_changed || !types) return;
    uint32_t cell_count;
    pthread_mutex_lock(&server->state.mutex);
    cell_count=(uint32_t)server->state.map.rows*server->state.map.cols;
    pthread_mutex_unlock(&server->state.mutex);

    for (uint32_t i=0;i<cell_count;i++) {
        if (!bonus_cells_changed[i]) continue;
        if (send_bonus_available(server,(uint16_t)i,types[i])!=0) printf("Error sending bonus available for cell %u\n",(unsigned)i);
    }
}

void send_bonus_retrieved_broadcast(server_t* server,bool* collected_players,uint16_t* collected_cells)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!collected_players[i]) continue;
        if (send_bonus_retrieved(server,i,collected_cells[i])!=0) printf("Error sending bonus retrieved for player %u\n",i);
    }
}

void send_block_destroyed_broadcast(server_t* server,bool* destroyed_cells)
{
    uint32_t cell_count;

    if (!server || !destroyed_cells) return;

    pthread_mutex_lock(&server->state.mutex);
    cell_count=(uint32_t)server->state.map.rows*server->state.map.cols;
    pthread_mutex_unlock(&server->state.mutex);

    for (uint32_t i=0;i<cell_count;i++) {
        if (!destroyed_cells[i]) continue;
        if (send_block_destroyed(server,(uint16_t)i)!=0) {
            printf("Error sending destroyed block for cell %u\n",(unsigned)i);
        }
    }
}
