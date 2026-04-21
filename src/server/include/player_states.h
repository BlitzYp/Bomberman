#ifndef PLAYER_STATES_H
#define PLAYER_STATES_H

#include "../../shared/include/protocol.h"
#include "server.h"
#include "./game_state.h"

int send_move(server_t* server, uint8_t slot_id);
int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header);
void send_move_broadcast(server_t* server, bool* moved_players);
void handle_action_move(server_t* server, player_slot_t* slot,action_t action);
int send_bomb(server_t* server, uint8_t slot_id);
int handle_bomb(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header);
void send_bomb_broadcast(server_t* server, bool* bomb_placed_players);
void send_exploding_broadcast(server_t* server, bool* exploding_bombs);
void send_end_explode_broadcast(server_t* server, bool* end_exploding_bombs);
void handle_action_bomb(server_t* server,bomb_t* slot,action_t action);
int send_explode_start(server_t* server,bomb_t* slot);
int send_explode_end(server_t* server,bomb_t* slot);
#endif
