#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include <stdint.h>

int connect_to_server(const char* ip, int port);
int send_hello(int fd, const char* client_id, const char* player_name);
int send_player_move(int fd, uint8_t dir);
int send_player_bomb(int fd);
int send_set_ready(int fd);
int send_leave(int fd);

#endif
