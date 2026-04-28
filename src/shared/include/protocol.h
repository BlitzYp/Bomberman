#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "config.h"

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
} msg_header_t;

typedef struct {
    msg_header_t header;
    char client_id[MAX_CLIENT_ID_LEN];
    char player_name[MAX_NAME_LEN];
} msg_hello_t;

typedef struct {
    msg_header_t header;
    uint8_t direction;
} msg_move_attempt_t;

typedef struct {
    msg_header_t header;
    uint16_t cell_index;
} msg_bomb_attempt_t;

typedef struct {
    msg_header_t header;
    uint8_t player_id;
    uint16_t cell_index;
} msg_moved_t;

typedef struct {
    msg_header_t header;
    uint8_t player_id;
    uint16_t cell_index;
} msg_bomb_t;

typedef struct {
    msg_header_t header;
    uint8_t player_id;
    uint16_t cell_index;
    uint8_t radius;
} msg_explosion_t;

typedef struct {
    msg_header_t header;
    uint8_t player_id;
} msg_death_t;

typedef struct {
    msg_header_t header;
    uint8_t player_id;
} msg_winner_t;

typedef struct {
    msg_header_t header;
} msg_set_ready_t;

typedef struct {
    msg_header_t header;
} msg_select_map_t;

typedef struct {
    msg_header_t header;
} msg_sync_request_t;

typedef struct {
    msg_header_t header;
    uint8_t status;
} msg_set_status_t;

typedef struct {
    msg_header_t header;
    char map_name[MAX_MAP_NAME_LEN];
} msg_selected_map_t;

typedef struct {
    msg_header_t header;
    uint8_t ready_count;
    uint8_t connected_count;
} msg_ready_state_t;

typedef struct {
    msg_header_t header;
    uint8_t bonus_type;
    uint16_t cell_index;
} msg_bonus_available_t;

typedef struct {
    msg_header_t header;
    uint8_t player_id;
    uint16_t cell_index;
} msg_bonus_retrieved_t;

typedef struct {
    msg_header_t header;
    uint16_t cell_index;
} msg_block_destroyed_t;
#endif
