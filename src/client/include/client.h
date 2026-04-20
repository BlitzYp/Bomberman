#ifndef CLIENT_H
#define CLIENT_H

#include "../../shared/include/config.h"

#include <stdint.h>

typedef struct {
    bool known;
    uint16_t row;
    uint16_t col;
} client_player_t;

typedef struct {
    uint16_t rows;
    uint16_t cols;
    tile_t* tiles;
    client_player_t players[MAX_PLAYERS];
} client_game_t;
#endif
