#include "../include/game_state.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>

int game_state_init(game_state_t* state)
{
    memset((void*)state,0,sizeof(*state));
    state->running=true;
    state->status=GAME_LOBBY;
    if (pthread_mutex_init(&state->mutex,NULL)!=0) return -1;
    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        state->players[i].id=i;
        state->players[i].socket_fd=-1;
        state->players[i].connected=false;
    }
    return 0;
}

void game_state_destroy(game_state_t* state)
{
    if (!state->running) pthread_mutex_destroy(&state->mutex);
}
