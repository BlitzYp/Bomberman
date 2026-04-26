#ifndef BOMBS_H
#define BOMBS_H

#include <stdbool.h>

#include "server.h"

void update_bomb_timers(server_t* server);
void collect_bomb_events(server_t* server, bool* exploding_bombs, bool* end_exploding_bombs);
void apply_explosion_start(server_t* server, bomb_t* bomb,bool* bonus_cells_changed,bonus_type_t* available_cell_types);
void apply_explosion_end(server_t* server, bomb_t* bomb);

#endif
