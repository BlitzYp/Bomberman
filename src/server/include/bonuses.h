#ifndef BONUSES_H
#define BONUSES_H

#include "server.h"

bonus_type_t get_bonus_at(game_state_t* state,uint16_t cell_index);
void clear_bonus_at(game_state_t* state,uint16_t cell_index);
int place_bonus(game_state_t* state,uint16_t cell_index,bonus_type_t type);

int apply_bonus_player(player_slot_t* slot,bonus_type_t type);
int collect_bonus_player(game_state_t* state,player_slot_t* slot,bonus_type_t* collected_type);
#endif
