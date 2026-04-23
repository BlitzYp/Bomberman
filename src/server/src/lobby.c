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

    for (uint8_t i=0;i<connected_count;i++) {
        if (send_move(server,connected_players[i])!=0) return -1;
    }

    printf("HELLO FROM %s SLOT %u\n",msg.player_name,slot_id);
    return 0;
}
