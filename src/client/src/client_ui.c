#include "../../shared/include/config.h"
#include "../include/client_ui.h"

#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#define FOOTER_STATUS_OFFSET 3
#define FOOTER_DETAIL_OFFSET 4
#define FOOTER_CONTROLS_OFFSET 5
#define FOOTER_EVENT_OFFSET 6

static const char* status_name(game_status_t status)
{
    switch (status) {
        case GAME_LOBBY:
            return "LOBBY";
        case GAME_RUNNING:
            return "RUNNING";
        case GAME_END:
            return "END";
        default:
            return "UNKNOWN";
    }
}

static char tile_to_char(tile_t tile)
{
    switch (tile) {
        case TILE_EMPTY:
            return ' ';
        case TILE_HARD_WALL:
            return '#';
        case TILE_SOFT_BLOCK:
            return '%';
        case TILE_BOMB:
            return 'O';
        case TILE_BOMB_EXPLODE:
            return '+';
        default:
            return '?';
    }
}

static char bonus_to_char(bonus_type_t bonus)
{
    switch (bonus) {
        case BONUS_SPEED:
            return 'S';
        case BONUS_RADIUS:
            return 'R';
        case BONUS_TIMER:
            return 'T';
        case BONUS_BOMB_COUNT:
            return 'N';
        case BONUS_NONE:
        default:
            return ' ';
    }
}

static void draw_footer_line(int row, const char* text)
{
    move(row,0);
    clrtoeol();
    if (text && text[0]!='\0') printw("%s",text);
}

static void draw_map(WINDOW* map_wind, const client_game_t* game)
{
    werase(map_wind);
    box(map_wind,0,0);

    for (uint16_t r=0;r<game->rows;r++) {
        for (uint16_t c=0;c<game->cols;c++) {
            uint16_t cell=make_cell_index(r,c,game->cols);
            mvwaddch(map_wind,r+1,c+1,tile_to_char(game->tiles[cell]));
        }
    }
}

static void draw_bonuses(WINDOW* map_wind, const client_game_t* game)
{
    if (!game->bonuses) return;

    for (uint16_t r=0;r<game->rows;r++) {
        for (uint16_t c=0;c<game->cols;c++) {
            uint16_t cell=make_cell_index(r,c,game->cols);
            bonus_type_t bonus=game->bonuses[cell];

            if (bonus==BONUS_NONE) continue;
            if (game->tiles[cell]!=TILE_EMPTY) continue;

            mvwaddch(map_wind,r+1,c+1,(chtype)bonus_to_char(bonus));
        }
    }
}

void client_ui_draw_footer(const client_game_t* game)
{
    char status_line[96];
    char detail_line[160];
    char controls_line[96];

    snprintf(status_line,sizeof(status_line),"Status: %s",status_name(game->status));

    detail_line[0]='\0';
    controls_line[0]='\0';

    if (game->has_winner) {
        const char* winner_name=game->players[game->winner_id].name;
        if (winner_name[0]=='\0') winner_name="Unknown";
        snprintf(detail_line,sizeof(detail_line),"Winner: %s",winner_name);
        snprintf(controls_line,sizeof(controls_line),"Press r to restart | q quit");
    }
    else if (game->status==GAME_LOBBY) {
        if (game->selected_map_name[0]!='\0') {
            snprintf(detail_line,sizeof(detail_line),"Map: %s",game->selected_map_name);
        }
        else {
            snprintf(detail_line,sizeof(detail_line),"Map: (unknown)");
        }
        snprintf(controls_line,sizeof(controls_line),"Lobby: [ ] change map | r ready | q quit");
    }
    else if (game->status==GAME_END) {
        snprintf(detail_line,sizeof(detail_line),"Game over");
        snprintf(controls_line,sizeof(controls_line),"Press r to restart | q quit");
    }
    else if (game->status==GAME_RUNNING && game->waiting_for_next_round) {
        snprintf(detail_line,sizeof(detail_line),"Waiting for next round");
        snprintf(controls_line,sizeof(controls_line),"q quit");
    }
    else {
        snprintf(controls_line,sizeof(controls_line),"Running: arrows move | space bomb | q quit");
    }

    draw_footer_line((int)game->rows+FOOTER_STATUS_OFFSET,status_line);
    draw_footer_line((int)game->rows+FOOTER_DETAIL_OFFSET,detail_line);
    draw_footer_line((int)game->rows+FOOTER_CONTROLS_OFFSET,controls_line);
    draw_footer_line((int)game->rows+FOOTER_EVENT_OFFSET,game->announcement);
    refresh();
}

void client_ui_redraw(WINDOW* map_wind, const client_game_t* game)
{
    draw_map(map_wind,game);
    draw_bonuses(map_wind,game);

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!game->players[i].known) continue;
        if (!game->players[i].alive) continue;
        mvwaddch(map_wind,game->players[i].row+1,game->players[i].col+1,(chtype)('1'+i));
    }

    wrefresh(map_wind);
    client_ui_draw_footer(game);
}

int client_ui_init(WINDOW** main_wind, WINDOW** map_wind, const client_game_t* game)
{
    if (!main_wind || !map_wind || !game) return -1;

    *main_wind=initscr();
    if (!*main_wind) return -1;

    *map_wind=newwin(game->rows+2, game->cols+2, 0, 0);
    if (!*map_wind) {
        endwin();
        return -1;
    }

    noecho();
    curs_set(0);
    keypad(*main_wind, TRUE);
    nodelay(*main_wind, TRUE);

    return 0;
}

int client_ui_sync_map_window(WINDOW** map_wind, const client_game_t* game)
{
    if (!map_wind || !game) return -1;

    if (*map_wind) {
        delwin(*map_wind);
        *map_wind=NULL;
    }

    clear();
    refresh();

    *map_wind=newwin(game->rows+2, game->cols+2, 0, 0);
    if (!*map_wind) return -1;

    return 0;
}

void client_ui_shutdown(WINDOW* main_wind, WINDOW* map_wind)
{
    delwin(map_wind);
    delwin(main_wind);
    endwin();
    refresh();
}

void client_ui_set_announcement(client_game_t* game, const char* message)
{
    if (!game) return;

    if (!message) {
        game->announcement[0]='\0';
        return;
    }

    snprintf(game->announcement,sizeof(game->announcement),"%s",message);
}
