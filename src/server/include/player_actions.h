#ifndef PLAYER_ACTIONS_H
#define PLAYER_ACTIONS_H

#include "../../shared/include/protocol.h"
#include "server.h"
#include "./game_state.h"

int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header);
void handle_action_move(server_t* server, player_slot_t* slot,action_t action);
int handle_bomb(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header);
bool handle_action_bomb(server_t* server,bomb_t* bomb,action_t action);
#endif
