#ifndef CLIENT_V2_PROTOCOL_H
#define CLIENT_V2_PROTOCOL_H

#include "../../client/include/client.h"

int client_v2_recv_welcome(int fd,client_game_t* game);
int client_v2_recv_map(int fd,client_game_t* game);
int client_v2_process_server_message(int fd,client_game_t* game);

#endif
