#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>
#include <pthread.h>
#include "../../shared/include/config.h"

typedef struct {
    bool connected;
    bool ready;
    bool alive;
    int socket_fd;
    uint8_t id;
    char name[MAX_NAME_LEN+1];
    player_t p;
} player_slot_t;

typedef struct {
    game_status_t status;
    bool running;
    player_slot_t players[MAX_PLAYERS];
    uint8_t player_count;
    pthread_mutex_t mutex;
} game_state_t;

int game_state_init(game_state_t* state);
void game_state_destroy(game_state_t* state);

#endif
