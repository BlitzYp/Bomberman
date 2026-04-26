#include "../include/bonuses.h"

bonus_type_t get_bonus_at(game_state_t *state, uint16_t cell_index)
{
    if (!state || !state->bonuses) return BONUS_NONE;

    uint32_t cell_count=state->map.rows*state->map.cols;
    if (cell_index>=cell_count) return BONUS_NONE;

    return state->bonuses[cell_index];
}

void clear_bonus_at(game_state_t *state, uint16_t cell_index)
{
    if (!state || !state->bonuses) return;
    uint32_t cell_count=state->map.rows*state->map.cols;
    if (cell_index>=cell_count) return;
    state->bonuses[cell_index]=BONUS_NONE;
}

int place_bonus(game_state_t *state, uint16_t cell_index, bonus_type_t type)
{
    if (!state || !state->bonuses) return -1;
    if (type==BONUS_NONE) return -1;

    uint32_t cell_count=state->map.rows*state->map.cols;
    if (cell_index>=cell_count) return -1;

    state->bonuses[cell_index]=type;
    return 0;
}

int apply_bonus_player(player_slot_t *slot, bonus_type_t type)
{
    if (!slot) return -1;

    switch(type) {
        case BONUS_SPEED:
            slot->p.speed++;
            return 0;
        case BONUS_RADIUS:
            slot->p.bomb_radius++;
            return 0;
        case BONUS_TIMER:
            // Remove one tick from bomb timer
            if (slot->p.bomb_timer_ticks>TICKS_PER_SECOND) slot->p.bomb_timer_ticks-=TICKS_PER_SECOND;
            return 0;
        case BONUS_NONE: {}
        default: return -1;
    }
}

// Return 0 for no bonus, 1 for impact bonuses
int collect_bonus_player(game_state_t *state, player_slot_t *slot, bonus_type_t *collected_type)
{
    if (!state || !slot || !collected_type) return -1;
    if (!slot->connected || !slot->alive) return -1;

    uint16_t cell_index=make_cell_index(slot->p.row,slot->p.col,state->map.cols);
    bonus_type_t bonus=get_bonus_at(state,cell_index);
    if (bonus==BONUS_NONE) return 0;

    if (apply_bonus_player(slot,bonus)!=0) return -1;
    clear_bonus_at(state,cell_index);
    *collected_type=bonus;
    return 1;
}
