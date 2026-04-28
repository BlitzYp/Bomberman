#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

#include "client.h"

#include <ncurses.h>

int recv_welcome(int fd, client_game_t* game);
int recv_map(int fd, client_game_t* game);
int process_server_message(int fd, WINDOW** map_wind, client_game_t* game);

#endif
