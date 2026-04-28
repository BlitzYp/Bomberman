#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PLAYERS 8
#define MAX_NAME_LEN 30
#define MAX_CLIENT_ID_LEN 20
#define MAX_MAP_NAME_LEN 64
#define TICKS_PER_SECOND 20
#define SERVER_TARGET_ID 255
#define BROADCAST_TARGET_ID 254
#define MAX_BOMBS 64
#define MAPS_DIR "src/assets/maps"

typedef enum {
    GAME_LOBBY = 0,
    GAME_RUNNING = 1,
    GAME_END = 2
} game_status_t;

typedef enum {
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3
} direction_t;

typedef enum {
    BONUS_NONE = 0,
    BONUS_SPEED = 1,
    BONUS_RADIUS = 2,
    BONUS_TIMER = 3,
    BONUS_BOMB_COUNT = 4
} bonus_type_t;

typedef enum {
    MSG_HELLO = 0,
    MSG_WELCOME = 1,
    MSG_DISCONNECT = 2,
    MSG_PING = 3,
    MSG_PONG = 4,
    MSG_LEAVE = 5,
    MSG_ERROR = 6,
    MSG_SET_READY = 10,
    MSG_SELECT_MAP_PREV = 11,
    MSG_SELECT_MAP_NEXT = 12,
    MSG_SET_STATUS = 20,
    MSG_SELECTED_MAP = 21,
    MSG_MAP = 7,
    MSG_WINNER = 23,
    MSG_MOVE_ATTEMPT = 30,
    MSG_BOMB_ATTEMPT = 31,
    MSG_MOVED = 40,
    MSG_BOMB = 41,
    MSG_EXPLOSION_START = 42,
    MSG_EXPLOSION_END = 43,
    MSG_DEATH = 44,
    MSG_BONUS_AVAILABLE = 45,
    MSG_BONUS_RETRIEVED = 46,
    MSG_BLOCK_DESTROYED = 47,
    MSG_SYNC_BOARD = 100,
    MSG_SYNC_REQUEST = 101
} msg_type_t;

typedef struct {
    uint8_t id;
    char name[MAX_NAME_LEN + 1];
    uint16_t row;
    uint16_t col;
    bool alive;
    bool ready;
    uint8_t bomb_count;
    uint8_t bomb_capacity;
    uint8_t bomb_radius;
    uint16_t bomb_timer_ticks;
    uint16_t speed;
} player_t;

typedef enum {
    BOMB_INACTIVE=0,
    BOMB_PLANTED=1,
    BOMB_EXPLODING=2
} bomb_state_t;

typedef struct {
    bomb_state_t state;
    uint8_t owner_id;
    uint16_t row;
    uint16_t col;
    uint8_t radius;
    uint16_t timer_ticks;
    uint16_t explosion_ticks;
} bomb_t;

typedef enum {
    TILE_EMPTY=0,
    TILE_HARD_WALL=1,
    TILE_SOFT_BLOCK=2,
    TILE_BOMB=3,
    TILE_BOMB_EXPLODE=4,
} tile_t;

typedef struct {
    uint16_t rows;
    uint16_t cols;
    tile_t* tiles;
    bonus_type_t* bonuses;
    uint16_t spawn_cells[MAX_PLAYERS];
    uint8_t spawn_count;
} map_t;

static inline uint16_t make_cell_index(uint16_t row, uint16_t col, uint16_t cols) {
    return (uint16_t)(row * cols + col);
}

#endif
