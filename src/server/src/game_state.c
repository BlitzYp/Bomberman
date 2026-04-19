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
    state->rows=13;
    state->cols=30;
    return 0;
}

void game_state_destroy(game_state_t* state)
{
    pthread_mutex_destroy(&state->mutex);
}

int enqueue_action(game_state_t* state,action_t action)
{
    action_queue_t* q=&state->action_queue;
    if (q->count>=ACTION_QUEUE_CAPACITY) return -1;

    q->items[q->tail]=action;
    q->tail=(q->tail+1)%ACTION_QUEUE_CAPACITY;
    q->count++;
    return 0;
}

int dequeue_action(game_state_t* state,action_t* action)
{
    action_queue_t* q=&state->action_queue;
    if (q->count<=0) return -1;

    *action=q->items[q->head];
    q->head=(q->head+1)%ACTION_QUEUE_CAPACITY;
    q->count--;
    return 0;
}
