#include "../../shared/include/config.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"
#include "../include/client_protocol.h"
#include "../include/client_ui.h"

#include <ncurses.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char* bonus_type_name(bonus_type_t bonus)
{
    switch (bonus) {
        case BONUS_SPEED:
            return "Speed";
        case BONUS_RADIUS:
            return "Radius";
        case BONUS_TIMER:
            return "Timer";
        case BONUS_BOMB_COUNT:
            return "Bomb count + 1";
        case BONUS_NONE:
        default:
            return "Unknown";
    }
}

int recv_welcome(int fd, client_game_t* game)
{
    msg_header_t header;
    char server_id[MAX_CLIENT_ID_LEN + 1];
    uint8_t status;
    uint8_t player_count;

    if (get_header(fd, &header) < 0) return -1;
    if (header.msg_type != MSG_WELCOME) return -1;

    if (read_exact(fd, server_id, MAX_CLIENT_ID_LEN) < 0) return -1;
    server_id[MAX_CLIENT_ID_LEN] = '\0';

    if (recv_u8(fd, &status) < 0) return -1;
    if (recv_u8(fd, &player_count) < 0) return -1;
    game->status=(game_status_t)status;

    game->local_player_id=header.sender_id;
    game->waiting_for_next_round=(game->status==GAME_RUNNING);
    printf("WELCOME\n");
    printf("assigned_id=%u\n", header.sender_id);
    printf("server_id=%s\n", server_id);
    printf("status=%u\n", status);
    printf("player_count=%u\n", player_count);

    for (uint8_t i=0;i<player_count; i++) {
        uint8_t id;
        uint8_t ready;
        char name[MAX_NAME_LEN + 1];

        if (recv_u8(fd, &id) < 0) return -1;
        if (recv_u8(fd, &ready) < 0) return -1;
        if (read_exact(fd, name, MAX_NAME_LEN) < 0) return -1;
        if (id>=MAX_PLAYERS) return -1;
        name[MAX_NAME_LEN] = '\0';

        printf("player[%u]: id=%u ready=%u name=%s\n", i, id, ready, name);
        strcpy(game->players[id].name,name);
        game->players[id].name[MAX_NAME_LEN]='\0';
    }

    return 0;
}

static int recv_hello_payload(int fd, msg_header_t header, client_game_t* game)
{
    char client_id[MAX_CLIENT_ID_LEN + 1];
    char name[MAX_NAME_LEN + 1];

    if (read_exact(fd,client_id,MAX_CLIENT_ID_LEN)<0) return -1;
    if (read_exact(fd,name,MAX_NAME_LEN)<0) return -1;
    if (header.sender_id>=MAX_PLAYERS) return -1;

    client_id[MAX_CLIENT_ID_LEN]='\0';
    name[MAX_NAME_LEN]='\0';
    strcpy(game->players[header.sender_id].name,name);
    game->players[header.sender_id].name[MAX_NAME_LEN]='\0';

    return 0;
}

static int recv_map_payload(int fd, client_game_t* game)
{
    uint16_t rows;
    uint16_t cols;
    size_t new_cell_count;
    size_t old_cell_count;
    bool resize_bonuses=false;

    if (recv_u16_be(fd, &rows)<0) return -1;
    if (recv_u16_be(fd, &cols)<0) return -1;
    if (rows==0 || cols==0) return -1;

    tile_t* tiles=malloc((size_t)rows*cols*sizeof(*tiles));
    if (!tiles) return -1;

    for (uint32_t i=0;i<(uint32_t)rows*cols;i++) {
        uint8_t tile;
        if (recv_u8(fd, &tile)<0) {
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
    game->rows=rows;
    game->cols=cols;
    game->tiles=tiles;

    return 0;
}

int recv_map(int fd, client_game_t* game)
{
    msg_header_t header;

    if (get_header(fd, &header)<0) return -1;
    if (header.msg_type!=MSG_MAP) return -1;

    return recv_map_payload(fd,game);
}

static int recv_player_move_payload(int fd, client_game_t* game)
{
    uint8_t player_id;
    uint16_t cell_index;
    uint16_t row;
    uint16_t col;

    if (recv_u8(fd, &player_id) < 0) return -1;
    if (recv_u16_be(fd, &cell_index) < 0) return -1;
    if (player_id>=MAX_PLAYERS || game->cols==0) return -1;

    row=cell_index/game->cols;
    col=cell_index%game->cols;
    if (row>=game->rows || col>=game->cols) return -1;

    game->players[player_id].known=true;
    game->players[player_id].alive=true;
    game->players[player_id].row=row;
    game->players[player_id].col=col;
    if (player_id==game->local_player_id) {
        game->waiting_for_next_round=false;
    }

    return 0;
}

static int recv_death_payload(int fd, client_game_t* game)
{
    uint8_t player_id;

    if (recv_u8(fd, &player_id) < 0) return -1;
    if (player_id>=MAX_PLAYERS) return -1;

    game->players[player_id].alive=false;
    if (player_id==game->local_player_id && game->status==GAME_RUNNING) {
        game->waiting_for_next_round=true;
    }

    return 0;
}

static int discard_bomb_payload(int fd)
{
    uint8_t player_id;
    uint16_t cell_index;

    if (recv_u8(fd, &player_id) < 0) return -1;
    if (recv_u16_be(fd, &cell_index) < 0) return -1;

    return 0;
}

static int discard_explosion_start_payload(int fd)
{
    uint8_t radius;
    uint16_t cell_index;

    if (recv_u8(fd, &radius) < 0) return -1;
    if (recv_u16_be(fd, &cell_index) < 0) return -1;

    return 0;
}

static int discard_explosion_end_payload(int fd)
{
    uint16_t cell_index;

    if (recv_u16_be(fd, &cell_index) < 0) return -1;

    return 0;
}

static int recv_winner_payload(int fd,client_game_t* game)
{
    uint8_t player_id;

    if (recv_u8(fd,&player_id)<0) return -1;
    if (player_id>=MAX_PLAYERS) return -1;

    game->has_winner=true;
    game->winner_id=player_id;
    client_ui_set_announcement(game,"");
    return 0;
}

static int recv_status_payload(int fd, client_game_t* game)
{
    uint8_t status;

    if (recv_u8(fd,&status)<0) return -1;
    if (status>GAME_END) return -1;

    game->status=(game_status_t)status;
    if (game->status!=GAME_END) {
        game->has_winner=false;
        game->winner_id=SERVER_TARGET_ID;
    }
    else {
        client_ui_set_announcement(game,"");
    }
    if (game->status==GAME_LOBBY) {
        game->waiting_for_next_round=false;
    }
    if (game->status==GAME_RUNNING && game->bonuses) {
        memset(game->bonuses,BONUS_NONE,(size_t)game->rows*game->cols*sizeof(*game->bonuses));
    }
    if (game->status!=GAME_END) {
        client_ui_set_announcement(game,"");
    }
    return 0;
}

static int recv_bonus_available_payload(int fd, client_game_t* game)
{
    uint8_t bonus_type;
    uint16_t cell_index;
    uint32_t cell_count;

    if (recv_u8(fd,&bonus_type)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;

    if (bonus_type>BONUS_BOMB_COUNT) return -1;
    if (!game->bonuses) return -1;

    cell_count=(uint32_t)game->rows*game->cols;
    if (cell_index>=cell_count) return -1;

    game->bonuses[cell_index]=(bonus_type_t)bonus_type;
    return 0;
}

static int recv_bonus_retrieved_payload(int fd, client_game_t* game)
{
    uint8_t player_id;
    uint16_t cell_index;
    uint32_t cell_count;
    const char* player_name;
    bonus_type_t bonus_type;
    char message[128];

    if (recv_u8(fd,&player_id)<0) return -1;
    if (recv_u16_be(fd,&cell_index)<0) return -1;

    if (player_id>=MAX_PLAYERS) return -1;
    if (!game->bonuses) return -1;

    cell_count=(uint32_t)game->rows*game->cols;
    if (cell_index>=cell_count) return -1;

    bonus_type=game->bonuses[cell_index];
    game->bonuses[cell_index]=BONUS_NONE;

    player_name=game->players[player_id].name;
    if (player_name[0]=='\0') player_name="Unknown";

    snprintf(message,sizeof(message),"%s collected %s bonus",player_name,bonus_type_name(bonus_type));
    client_ui_set_announcement(game,message);
    return 0;
}

static int recv_block_destroyed_payload(int fd, client_game_t* game)
{
    uint16_t cell_index;
    uint32_t cell_count;

    if (recv_u16_be(fd,&cell_index)<0) return -1;
    if (!game->tiles) return -1;

    cell_count=(uint32_t)game->rows*game->cols;
    if (cell_index>=cell_count) return -1;

    return 0;
}

int process_server_message(int fd, WINDOW* map_wind, client_game_t* game)
{
    msg_header_t header;

    if (get_header(fd, &header)<0) return -1;

    switch (header.msg_type) {
        case MSG_DISCONNECT:
            return 1;
        case MSG_HELLO:
            if (recv_hello_payload(fd,header,game)!=0) return -1;
            return 0;
        case MSG_SET_STATUS:
            if (recv_status_payload(fd,game)!=0) return -1;
            client_ui_draw_footer(game);
            return 0;
        case MSG_LEAVE:
            if (header.sender_id>=MAX_PLAYERS) return -1;
            game->players[header.sender_id].known=false;
            game->players[header.sender_id].alive=false;
            client_ui_redraw(map_wind,game);
            return 0;
        case MSG_MAP:
            if (recv_map_payload(fd,game)<0) return -1;
            client_ui_redraw(map_wind,game);
            return 0;
        case MSG_MOVED:
            if (recv_player_move_payload(fd,game)!=0) return -1;
            client_ui_redraw(map_wind,game);
            return 0;
        case MSG_BOMB:
            if (discard_bomb_payload(fd)!=0) return -1;
            return 0;
        case MSG_EXPLOSION_START:
            if (discard_explosion_start_payload(fd)!=0) return -1;
            return 0;
        case MSG_EXPLOSION_END:
            if (discard_explosion_end_payload(fd)!=0) return -1;
            return 0;
        case MSG_DEATH:
            if (recv_death_payload(fd,game)!=0) return -1;
            client_ui_redraw(map_wind,game);
            return 0;
        case MSG_WINNER:
            if (recv_winner_payload(fd,game)!=0) return -1;
            client_ui_draw_footer(game);
            return 0;
        case MSG_BONUS_AVAILABLE:
            if (recv_bonus_available_payload(fd,game)!=0) return -1;
            client_ui_redraw(map_wind,game);
            return 0;
        case MSG_BONUS_RETRIEVED:
            if (recv_bonus_retrieved_payload(fd,game)!=0) return -1;
            client_ui_redraw(map_wind,game);
            return 0;
        case MSG_BLOCK_DESTROYED:
            if (recv_block_destroyed_payload(fd,game)!=0) return -1;
            return 0;
        default:
            return -1;
    }
}
