#include "../include/game_state.h"
#include "../../shared/include/map_loader.h"
#include "../../shared/include/config.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int map_clone(map_t* dst, const map_t* src)
{
    if (!dst || !src || !src->tiles || !src->bonuses) return -1;

    memset(dst,0,sizeof(*dst));
    dst->rows=src->rows;
    dst->cols=src->cols;
    dst->spawn_count=src->spawn_count;
    memcpy(dst->spawn_cells,src->spawn_cells,sizeof(dst->spawn_cells));

    dst->tiles=malloc((size_t)src->rows*src->cols*sizeof(*dst->tiles));
    if (!dst->tiles) {
        memset(dst,0,sizeof(*dst));
        return -1;
    }
    dst->bonuses=malloc((size_t)src->rows*src->cols*sizeof(*dst->bonuses));
    if (!dst->bonuses) {
        free(dst->tiles);
        memset(dst,0,sizeof(*dst));
        return -1;
    }

    memcpy(dst->tiles,src->tiles,(size_t)src->rows*src->cols*sizeof(*dst->tiles));
    memcpy(dst->bonuses,src->bonuses,(size_t)src->rows*src->cols*sizeof(*dst->bonuses));
    return 0;
}

static int init_bonus_storage(game_state_t* state)
{
    size_t cell_count;

    if (!state || !state->map.tiles) return -1;

    cell_count=(size_t)state->map.rows*state->map.cols;
    state->bonuses=malloc(cell_count*sizeof(*state->bonuses));
    if (!state->bonuses) return -1;

    memcpy(state->bonuses,state->map.bonuses,cell_count*sizeof(*state->bonuses));
    return 0;
}

static void reset_player_for_round(player_slot_t* slot, const map_t* map)
{
    slot->ready=false;
    slot->p.ready=false;

    if (!slot->connected) {
        slot->alive=false;
        slot->p.alive=false;
        return;
    }

    slot->alive=true;
    slot->p.alive=true;
    slot->p.bomb_count=1;
    slot->p.bomb_radius=1;
    slot->p.bomb_timer_ticks=3*TICKS_PER_SECOND;
    slot->p.speed=3;

    if (slot->id<map->spawn_count && map->spawn_cells[slot->id]!=UINT16_MAX) {
        uint16_t cell=map->spawn_cells[slot->id];
        slot->p.row=cell/map->cols;
        slot->p.col=cell%map->cols;
    }
    else {
        slot->p.row=1;
        slot->p.col=(uint16_t)(1+slot->id);
    }
}

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

    if (map_load_from_file("src/assets/maps/sample_map.txt",&state->map)!=0) {
        pthread_mutex_destroy(&state->mutex);
        return -1;
    }
    if (map_clone(&state->initial_map,&state->map)!=0) {
        map_destroy(&state->map);
        pthread_mutex_destroy(&state->mutex);
        return -1;
    }
    if (init_bonus_storage(state)!=0) {
        map_destroy(&state->initial_map);
        map_destroy(&state->map);
        pthread_mutex_destroy(&state->mutex);
        return -1;
    }
    return 0;
}

void game_state_destroy(game_state_t* state)
{
    pthread_mutex_destroy(&state->mutex);
    free(state->bonuses);
    map_destroy(&state->map);
    map_destroy(&state->initial_map);
}

int game_state_reset_round(game_state_t* state)
{
    if (!state || !state->map.tiles || !state->initial_map.tiles) return -1;
    if (state->map.rows!=state->initial_map.rows || state->map.cols!=state->initial_map.cols) return -1;

    // Tiles
    memcpy(state->map.tiles,state->initial_map.tiles,(size_t)state->initial_map.rows*state->initial_map.cols*sizeof(*state->map.tiles));
    // Map bonuses
    memcpy(state->map.bonuses,state->initial_map.bonuses,(size_t)state->initial_map.rows*state->initial_map.cols*sizeof(*state->map.bonuses));
    state->map.rows=state->initial_map.rows;
    state->map.cols=state->initial_map.cols;
    state->map.spawn_count=state->initial_map.spawn_count;
    // Spawn locations
    memcpy(state->map.spawn_cells,state->initial_map.spawn_cells,sizeof(state->map.spawn_cells));

    memset(state->bombs,0,sizeof(state->bombs));
    state->bomb_count=0;
    // Game state bonuses
    memcpy(state->bonuses,state->initial_map.bonuses,(size_t)state->initial_map.rows*state->initial_map.cols*sizeof(*state->bonuses));

    state->action_queue.head=0;
    state->action_queue.tail=0;
    state->action_queue.count=0;

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        reset_player_for_round(&state->players[i],&state->map);
    }

    state->status=GAME_RUNNING;
    return 0;
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
