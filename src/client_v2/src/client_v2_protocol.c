#include "../include/client_v2_protocol.h"
#include "../../shared/include/net_utils.h"
#include "../../shared/include/protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* player_name_or_unknown(const client_game_t* game,uint8_t player_id)
{
    if (!game || player_id>=MAX_PLAYERS) return "Unknown";
    if (game->players[player_id].name[0]=='\0') return "Unknown";
    return game->players[player_id].name;
}

int client_v2_recv_welcome(int fd,client_game_t* game)
{
    msg_header_t header;
    char server_id[MAX_CLIENT_ID_LEN+1];
    uint8_t status;
    uint8_t player_count;

    if (!game) return -1;
    if (get_header(fd,&header)<0) return -1;
    if (header.msg_type!=MSG_WELCOME) return -1;
    if (read_exact(fd,server_id,MAX_CLIENT_ID_LEN)<0) return -1;
    server_id[MAX_CLIENT_ID_LEN]='\0';
    if (recv_u8(fd,&status)<0) return -1;
    if (recv_u8(fd,&player_count)<0) return -1;

    game->status=(game_status_t)status;
    game->local_player_id=header.sender_id;
    game->waiting_for_next_round=(game->status==GAME_RUNNING);

    for (uint8_t i=0;i<player_count;i++) {
        uint8_t id;
        uint8_t ready;
        char name[MAX_NAME_LEN+1];

        if (recv_u8(fd,&id)<0) return -1;
        if (recv_u8(fd,&ready)<0) return -1;
        if (read_exact(fd,name,MAX_NAME_LEN)<0) return -1;
        if (id>=MAX_PLAYERS) return -1;

        name[MAX_NAME_LEN]='\0';
        strcpy(game->players[id].name,name);
        game->players[id].name[MAX_NAME_LEN]='\0';
        game->players[id].known=true;
    }

    return 0;
}

int client_v2_recv_map(int fd,client_game_t* game)
{
    msg_header_t header;
    uint16_t rows;
    uint16_t cols;
    tile_t* tiles;

    if (!game) return -1;
    if (get_header(fd,&header)<0) return -1;
    if (header.msg_type!=MSG_MAP) return -1;
    if (recv_u16_be(fd,&rows)<0) return -1;
    if (recv_u16_be(fd,&cols)<0) return -1;
    if (rows==0 || cols==0) return -1;

    tiles=malloc((size_t)rows*cols*sizeof(*tiles));
    if (!tiles) return -1;

    for (uint32_t i=0;i<(uint32_t)rows*cols;i++) {
        uint8_t tile;
        if (recv_u8(fd,&tile)<0) {
            free(tiles);
            return -1;
        }
        if (tile>TILE_BOMB_EXPLODE) {
            free(tiles);
            return -1;
        }
        tiles[i]=(tile_t)tile;
    }

    free(game->tiles);
    game->tiles=tiles;
    game->rows=rows;
    game->cols=cols;
    free(game->bonuses);
    game->bonuses=calloc((size_t)rows*cols,sizeof(*game->bonuses));
    if (!game->bonuses) {
        free(game->tiles);
        game->tiles=NULL;
        game->rows=0;
        game->cols=0;
        return -1;
    }

    return 0;
}

static int recv_hello_payload(int fd,msg_header_t header,client_game_t* game)
{
    char client_id[MAX_CLIENT_ID_LEN+1];
    char name[MAX_NAME_LEN+1];

    if (!game) return -1;
    if (read_exact(fd,client_id,MAX_CLIENT_ID_LEN)<0) return -1;
    if (read_exact(fd,name,MAX_NAME_LEN)<0) return -1;
    if (header.sender_id>=MAX_PLAYERS) return -1;

    client_id[MAX_CLIENT_ID_LEN]='\0';
    name[MAX_NAME_LEN]='\0';
    strcpy(game->players[header.sender_id].name,name);
    game->players[header.sender_id].name[MAX_NAME_LEN]='\0';
    game->players[header.sender_id].known=true;

    snprintf(game->announcement,sizeof(game->announcement),"Player %s joined!",player_name_or_unknown(game,header.sender_id));
    return 0;
}

static int recv_selected_map_payload(int fd,client_game_t* game)
{
    if (!game) return -1;
    if (read_exact(fd,game->selected_map_name,MAX_MAP_NAME_LEN)<0) return -1;
    game->selected_map_name[MAX_MAP_NAME_LEN-1]='\0';
    game->announcement[0]='\0';
    return 0;
}

static int recv_map_payload(int fd,client_game_t* game)
{
    uint16_t rows;
    uint16_t cols;
    size_t new_cell_count;
    size_t old_cell_count;
    bool resize_bonuses;
    tile_t* tiles;

    if (!game) return -1;
    if (recv_u16_be(fd,&rows)<0) return -1;
    if (recv_u16_be(fd,&cols)<0) return -1;
    if (rows==0 || cols==0) return -1;

    tiles=malloc((size_t)rows*cols*sizeof(*tiles));
    if (!tiles) return -1;

    for (uint32_t i=0;i<(uint32_t)rows*cols;i++) {
        uint8_t tile;
        if (recv_u8(fd,&tile)<0) {
            free(tiles);
            return -1;
        }
        if (tile>TILE_BOMB_EXPLODE) {
            free(tiles);
            return -1;
        }
        tiles[i]=(tile_t)tile;
    }

    new_cell_count=(size_t)rows*cols;
    old_cell_count=(size_t)game->rows*game->cols;
    resize_bonuses=(game->bonuses==NULL || new_cell_count!=old_cell_count);
    if (resize_bonuses) {
        bonus_type_t* bonuses=calloc(new_cell_count,sizeof(*bonuses));
        if (!bonuses) {
            free(tiles);
            return -1;
        }
        free(game->bonuses);
        game->bonuses=bonuses;
    }

    free(game->tiles);
    game->tiles=tiles;
    game->rows=rows;
    game->cols=cols;

    return 0;
}

static int recv_player_move_payload(int fd,client_game_t* game)
{
    uint8_t player_id;
    uint16_t cell_index;
    uint16_t row;
    uint16_t col;

    if (!game || game->cols==0) return -1;
    if (recv_u8(fd,&player_id)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;
    if (player_id>=MAX_PLAYERS) return -1;

    row=cell_index/game->cols;
    col=cell_index%game->cols;
    if (row>=game->rows || col>=game->cols) return -1;

    game->players[player_id].known=true;
    game->players[player_id].alive=true;
    game->players[player_id].row=row;
    game->players[player_id].col=col;
    if (player_id==game->local_player_id) game->waiting_for_next_round=false;

    return 0;
}

static int recv_death_payload(int fd,client_game_t* game)
{
    uint8_t player_id;

    if (!game) return -1;
    if (recv_u8(fd,&player_id)<0) return -1;
    if (player_id>=MAX_PLAYERS) return -1;

    game->players[player_id].alive=false;
    if (player_id==game->local_player_id && game->status==GAME_RUNNING) game->waiting_for_next_round=true;
    snprintf(game->announcement,sizeof(game->announcement),"Player %s died!",player_name_or_unknown(game,player_id));
    return 0;
}

static int recv_status_payload(int fd,client_game_t* game)
{
    uint8_t status;

    if (!game) return -1;
    if (recv_u8(fd,&status)<0) return -1;
    if (status>GAME_END) return -1;

    game->status=(game_status_t)status;
    if (game->status!=GAME_END) {
        game->has_winner=false;
        game->winner_id=SERVER_TARGET_ID;
        game->announcement[0]='\0';
    } else game->announcement[0]='\0';
    if (game->status==GAME_LOBBY) game->waiting_for_next_round=false;
    return 0;
}

static int recv_winner_payload(int fd,client_game_t* game)
{
    uint8_t player_id;

    if (!game) return -1;
    if (recv_u8(fd,&player_id)<0) return -1;
    if (player_id>=MAX_PLAYERS) return -1;

    game->has_winner=true;
    game->winner_id=player_id;
    game->announcement[0]='\0';
    return 0;
}

static int recv_bonus_available_payload(int fd,client_game_t* game)
{
    uint8_t bonus_type;
    uint16_t cell_index;
    uint32_t cell_count;

    if (!game || !game->bonuses) return -1;
    if (recv_u8(fd,&bonus_type)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;
    if (bonus_type>BONUS_BOMB_COUNT) return -1;

    cell_count=(uint32_t)game->rows*game->cols;
    if (cell_index>=cell_count) return -1;

    game->bonuses[cell_index]=(bonus_type_t)bonus_type;
    return 0;
}

static int recv_bonus_retrieved_payload(int fd,client_game_t* game)
{
    uint8_t player_id;
    uint16_t cell_index;
    uint32_t cell_count;

    if (!game || !game->bonuses) return -1;
    if (recv_u8(fd,&player_id)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;
    if (player_id>=MAX_PLAYERS) return -1;

    cell_count=(uint32_t)game->rows*game->cols;
    if (cell_index>=cell_count) return -1;

    game->bonuses[cell_index]=BONUS_NONE;
    return 0;
}

static int recv_block_destroyed_payload(int fd,client_game_t* game)
{
    uint16_t cell_index;
    uint32_t cell_count;

    if (!game || !game->tiles) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;

    cell_count=(uint32_t)game->rows*game->cols;
    if (cell_index>=cell_count) return -1;
    return 0;
}

static int recv_leave_header(msg_header_t header,client_game_t* game)
{
    if (!game || header.sender_id>=MAX_PLAYERS) return -1;
    game->players[header.sender_id].known=false;
    game->players[header.sender_id].alive=false;


    snprintf(game->announcement,sizeof(game->announcement),"Player %s left!",player_name_or_unknown(game,header.sender_id));
    return 0;
}

static int discard_bomb_payload(int fd)
{
    uint8_t player_id;
    uint16_t cell_index;

    if (recv_u8(fd,&player_id)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;
    return 0;
}

static int discard_explosion_start_payload(int fd)
{
    uint8_t radius;
    uint16_t cell_index;

    if (recv_u8(fd,&radius)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;
    return 0;
}

static int discard_explosion_end_payload(int fd)
{
    uint16_t cell_index;

    if (recv_u16_be(fd,&cell_index)<0) return -1;
    return 0;
}

int client_v2_process_server_message(int fd,client_game_t* game)
{
    msg_header_t header;

    if (!game) return -1;
    if (get_header(fd,&header)<0) return -1;

    switch (header.msg_type) {
        case MSG_DISCONNECT:
            return 1;
        case MSG_HELLO:
            return recv_hello_payload(fd,header,game);
        case MSG_SET_STATUS:
            return recv_status_payload(fd,game);
        case MSG_SELECTED_MAP:
            return recv_selected_map_payload(fd,game);
        case MSG_LEAVE:
            return recv_leave_header(header,game);
        case MSG_MAP:
            return recv_map_payload(fd,game);
        case MSG_MOVED:
            return recv_player_move_payload(fd,game);
        case MSG_BOMB:
            return discard_bomb_payload(fd);
        case MSG_EXPLOSION_START:
            return discard_explosion_start_payload(fd);
        case MSG_EXPLOSION_END:
            return discard_explosion_end_payload(fd);
        case MSG_DEATH:
            return recv_death_payload(fd,game);
        case MSG_WINNER:
            return recv_winner_payload(fd,game);
        case MSG_BONUS_AVAILABLE:
            return recv_bonus_available_payload(fd,game);
        case MSG_BONUS_RETRIEVED:
            return recv_bonus_retrieved_payload(fd,game);
        case MSG_BLOCK_DESTROYED:
            return recv_block_destroyed_payload(fd,game);
        default:
            return -1;
    }
}
