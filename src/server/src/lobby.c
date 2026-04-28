#include "../include/lobby.h"
#include "../include/server_messages.h"
#include "../../shared/include/net_utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool check_unique_name(server_t* server, char* name, uint8_t slot_id)
{
    if (!name || !server) return false;
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* player=&server->state.players[i];
        if (!player->connected || i==slot_id) continue;
        char* check=player->name;
        if (strncmp(name,check,MAX_NAME_LEN)==0) return false;
    }
    return true;
}

static bool is_lobby_owner(server_t* server,uint8_t slot_id)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p=&server->state.players[i];
        if (!p->connected) continue;
        return i==slot_id;
    }
    return false;
}

static void clear_ready_status(game_state_t* state)
{
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p=&state->players[i];
        if (!p->connected) continue;
        p->ready=false;
        p->p.ready=false;
    }
}

static int reload_map(server_t* server)
{
    if (!server) return -1;
    if (game_state_load_selected_map(&server->state)!=0) return -1;
    clear_ready_status(&server->state);
    return 0;
}

int lobby_select_next_map(server_t *server, uint8_t slot_id)
{
    if (!server || slot_id>=MAX_PLAYERS) return -1;

    pthread_mutex_lock(&server->state.mutex);
    if (server->state.status!=GAME_LOBBY) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    if (!is_lobby_owner(server,slot_id)) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    if (server->state.map_config_count==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }

    server->state.selected_map_index++;
    if (server->state.selected_map_index>=server->state.map_config_count) server->state.selected_map_index=0;

    if (reload_map(server)!=0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (broadcast_map(server)!=0) return -1;
    if (broadcast_all_bonuses(server)!=0) return -1;
    if (broadcast_selected_map(server)!=0) return -1;

    return 0;
}

int lobby_select_prev_map(server_t *server, uint8_t slot_id)
{

    if (!server || slot_id>=MAX_PLAYERS) return -1;

    pthread_mutex_lock(&server->state.mutex);
    if (server->state.status!=GAME_LOBBY) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    if (!is_lobby_owner(server,slot_id)) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    if (server->state.map_config_count==0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }

    if (server->state.selected_map_index==0) server->state.selected_map_index=(uint8_t)(server->state.map_config_count-1);
    else server->state.selected_map_index--;

    if (reload_map(server)!=0) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (broadcast_map(server)!=0) return -1;
    if (broadcast_all_bonuses(server)!=0) return -1;
    if (broadcast_selected_map(server)!=0) return -1;

    return 0;
}

int handle_hello(server_t* server, int client_fd, uint8_t slot_id, msg_header_t header)
{
    msg_hello_t msg;
    uint8_t connected_count=0;
    uint8_t connected_players[MAX_PLAYERS];

    msg.header=header;
    if (read_exact(client_fd,msg.client_id,MAX_CLIENT_ID_LEN)<0) return -1;
    if (read_exact(client_fd,msg.player_name,MAX_NAME_LEN)<0) return -1;

    pthread_mutex_lock(&server->state.mutex);
    // TODO: Implement check if the name is valid
    bool valid_name=check_unique_name(server,msg.player_name,slot_id);
    if (valid_name) {
        memcpy(server->state.players[slot_id].name,msg.player_name,MAX_NAME_LEN);
        server->state.players[slot_id].name[MAX_NAME_LEN]='\0';
        for (uint8_t i=0;i<MAX_PLAYERS;i++) {
            if (!server->state.players[i].connected) continue;
            connected_players[connected_count++]=i;
        }
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (!valid_name) return -1;
    if (send_welcome(server,client_fd,slot_id)!=0) return -1;
    if (send_map(client_fd,slot_id,&server->state.map)!=0) return -1;
    if (send_all_bonuses(server,client_fd,slot_id)!=0) return -1;
    if (send_selected_map(client_fd,slot_id,game_state_selected_map_name(&server->state))!=0) return -1;
    if (send_hello_broadcast(server,slot_id,msg.client_id,msg.player_name)!=0) return -1;

    for (uint8_t i=0;i<connected_count;i++) {
        uint8_t player_id=connected_players[i];
        // Handle situation when a new player joins mid game
        pthread_mutex_lock(&server->state.mutex);
        bool is_in_game=server->state.players[player_id].connected && server->state.players[player_id].alive;
        pthread_mutex_unlock(&server->state.mutex);
        if (is_in_game && send_move(server,connected_players[i])!=0) return -1;
    }

    printf("HELLO FROM %s SLOT %u\n",msg.player_name,slot_id);
    return 0;
}

int handle_set_ready(server_t* server,uint8_t slot_id)
{
    if (!server || slot_id>=MAX_PLAYERS) return -1;
    bool should_start=false;

    pthread_mutex_lock(&server->state.mutex);
    if (server->state.status!=GAME_LOBBY && server->state.status!=GAME_END) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }

    player_slot_t* slot=&server->state.players[slot_id];
    if (!slot->connected) {
        pthread_mutex_unlock(&server->state.mutex);
        return -1;
    }
    slot->ready=true;
    slot->p.ready=true;

    bool all_ready=true;
    uint8_t connected=0;
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        player_slot_t* p=&server->state.players[i];
        if (!p->connected) continue;
        connected++;
        if (!p->ready) all_ready=false;
    }

    if (connected>=2 && all_ready) {
        if (game_state_reset_round(&server->state)!=0) {
            pthread_mutex_unlock(&server->state.mutex);
            return -1;
        }
        should_start=true;
    }
    pthread_mutex_unlock(&server->state.mutex);

    if (should_start) {
        if (send_status_broadcast(server,GAME_RUNNING)<0) return -1;
        if (broadcast_map(server)<0) return -1;
        if (broadcast_all_bonuses(server)<0) return -1;
        for (uint8_t i=0;i<MAX_PLAYERS;i++) {
            if (send_move(server,i)!=0) continue;
        }
    }

    return 0;
}
