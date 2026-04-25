#ifndef LOBBY_H
#define LOBBY_H

#include "server.h"

int handle_hello(server_t* server, int client_fd, uint8_t slot_id, msg_header_t header);
int handle_set_ready(server_t* server,uint8_t slot_id);
#endif
