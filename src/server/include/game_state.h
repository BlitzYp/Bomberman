#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>
#include <pthread.h>
#include "../../shared/include/config.h"

typedef enum {
    ACTION_MOVE=0,
    ACTION_BOMB=1
} action_type_t;

typedef struct {
    uint8_t player_id;
    action_type_t type;
    uint8_t direction; // for moving
    uint16_t cell_index; // for bombs
} action_t;

typedef struct {
    bool connected;
    bool ready;
    bool alive;
    int socket_fd;
    uint8_t id;
    char name[MAX_NAME_LEN+1];
    player_t p;
} player_slot_t;

#define ACTION_QUEUE_CAPACITY 256
typedef struct {
    action_t items[ACTION_QUEUE_CAPACITY];
    int head,tail;
    int count;
} action_queue_t;

typedef struct {
    char name[64];
    char path[512];
} map_config_t;

#define MAX_MAP_CONFIGS 16
typedef struct {
    game_status_t status;
    bool running;
    player_slot_t players[MAX_PLAYERS];
    uint8_t player_count;
    bomb_t bombs[MAX_BOMBS];
    action_queue_t action_queue;
    pthread_mutex_t mutex;
    map_t map;
    map_t initial_map;
    bonus_type_t* bonuses;
    map_config_t map_configs[MAX_MAP_CONFIGS];
    uint8_t map_config_count;
    uint8_t selected_map_index;
} game_state_t;


int game_state_init(game_state_t* state);
void game_state_destroy(game_state_t* state);
int game_state_start_round(game_state_t* state);
int game_state_reset_round(game_state_t* state);
int game_state_load_selected_map(game_state_t* state);

int enqueue_action(game_state_t* state,action_t action);
int dequeue_action(game_state_t* state,action_t* action);

static inline const char* game_state_selected_map_name(const game_state_t* state)
{
    if (!state || state->map_config_count==0 || state->selected_map_index>=state->map_config_count) return "";
    return state->map_configs[state->selected_map_index].name;
}

#endif
