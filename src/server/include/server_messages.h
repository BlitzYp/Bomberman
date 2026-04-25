#ifndef SERVER_MESSAGES_H
#define SERVER_MESSAGES_H

#include <stdbool.h>
#include <stdint.h>

#include "server.h"

int send_welcome(server_t* server, int client_fd, uint8_t slot_id);
int send_disconnect(int client_fd, uint8_t target_id);
int send_leave_broadcast(server_t* server, uint8_t leaving_player_id);

int send_map(int client_fd, uint8_t target_id, const map_t* map);
int broadcast_map(server_t* server);

int send_move(server_t* server, uint8_t slot_id);
void send_move_broadcast(server_t* server, bool* moved_players);

int send_bomb(server_t* server, uint8_t slot_id);
void send_bomb_broadcast(server_t* server, bool* bomb_placed_players);

int send_explode_start(server_t* server, bomb_t* bomb);
int send_explode_end(server_t* server, bomb_t* bomb);
void send_exploding_broadcast(server_t* server, bool* exploding_bombs);
void send_end_explode_broadcast(server_t* server, bool* end_exploding_bombs);

int send_death(server_t* server, uint8_t player_id);
void send_death_broadcast(server_t* server, bool* dead_players);

int send_winner_broadcast(server_t* server, uint8_t player_id);

int send_status_broadcast(server_t* server,game_status_t status);
#endif
