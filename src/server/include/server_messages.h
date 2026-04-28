#ifndef SERVER_MESSAGES_H
#define SERVER_MESSAGES_H

#include <stdbool.h>
#include <stdint.h>

#include "server.h"

int send_welcome(server_t* server, int client_fd, uint8_t slot_id);
int send_disconnect(int client_fd, uint8_t target_id);
int send_leave_broadcast(server_t* server, uint8_t leaving_player_id);
int send_hello_broadcast(server_t* server, uint8_t joined_player_id, const char* client_id, const char* player_name);

int send_map(int client_fd, uint8_t target_id, const map_t* map);
int broadcast_map(server_t* server);

int send_move(server_t* server, uint8_t slot_id);
void send_move_broadcast(server_t* server, bool* moved_players);

int send_bomb(server_t* server, bomb_t* bomb);
void send_bomb_broadcast(server_t* server, bool* placed_bombs);

int send_explode_start(server_t* server, bomb_t* bomb);
int send_explode_end(server_t* server, bomb_t* bomb);
void send_exploding_broadcast(server_t* server, bool* started_bombs);
void send_end_explode_broadcast(server_t* server, bool* ended_bombs);

int send_death(server_t* server, uint8_t player_id);
void send_death_broadcast(server_t* server, bool* dead_players);

int send_winner_broadcast(server_t* server, uint8_t player_id);

int send_status_broadcast(server_t* server,game_status_t status);
int send_selected_map(int client_fd,uint8_t target_id,const char* map_name);
int broadcast_selected_map(server_t* server);
int send_full_sync(server_t* server,int client_fd,uint8_t target_id);

int send_bonus_available(server_t* server,uint16_t cell_index,bonus_type_t bonus);
int send_bonus_retrieved(server_t* server,uint8_t player_id,uint16_t cell_index);
int send_block_destroyed(server_t* server,uint16_t cell_index);
int send_all_bonuses(server_t* server,int client_fd,uint8_t target_id);
int broadcast_all_bonuses(server_t* server);

void send_bonus_available_broadcast(server_t *server, bool* bonus_cells_changed,bonus_type_t* types);
void send_bonus_retrieved_broadcast(server_t* server,bool* collected_players,uint16_t* collected_cells);
void send_block_destroyed_broadcast(server_t* server,bool* destroyed_cells);
#endif
