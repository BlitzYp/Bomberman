#ifndef CLIENT_H
#define CLIENT_H

#include "../../shared/include/config.h"

#include <stdint.h>

typedef struct {
    bool known;
    bool alive;
    uint16_t row;
    uint16_t col;
    char name[MAX_NAME_LEN+1];
} client_player_t;

typedef struct {
    uint16_t rows;
    uint16_t cols;
    tile_t* tiles;
    bonus_type_t* bonuses;
    client_player_t players[MAX_PLAYERS];
    game_status_t status;
    bool has_winner;
    uint8_t winner_id;
    uint8_t local_player_id;
    bool waiting_for_next_round;
} client_game_t;
#endif
