#ifndef LOBBY_H
#define LOBBY_H

#include "server.h"
#include "../../shared/include/protocol.h"

int handle_hello(server_t* server, int client_fd, uint8_t slot_id, msg_header_t header);
int handle_set_ready(server_t* server,uint8_t slot_id);

int lobby_select_next_map(server_t* server,uint8_t slot_id);
int lobby_select_prev_map(server_t* server,uint8_t slot_id);
#endif
