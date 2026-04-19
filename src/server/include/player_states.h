#ifndef PLAYER_STATES_H
#define PLAYER_STATES_H

#include "../../shared/include/protocol.h"

typedef struct server server_t;

int send_move(server_t* server, uint8_t slot_id);
int handle_move(server_t* server,int client_fd,uint8_t slot_id,msg_header_t header);

#endif
