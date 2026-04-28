#include "../../shared/include/config.h"
#include "../include/client_ui.h"

#include <ncurses.h>

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
        case BONUS_NONE:
        default:
            return ' ';
    }
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
    move((int)game->rows+3,0);
    clrtoeol();

    if (game->has_winner) {
        const char* winner_name=game->players[game->winner_id].name;
        if (winner_name[0]=='\0') winner_name="Unknown";
        printw("Game over! Winner is %s! Press r to restart",winner_name);
    }
    else if (game->status==GAME_LOBBY) {
        printw("Status: %s | Press r to ready",status_name(game->status));
    }
    else if (game->status==GAME_END) {
        printw("Game over! Press r to restart");
    }
    else if (game->status==GAME_RUNNING && game->waiting_for_next_round) {
        printw("Status: GAME IN PROGRESS | Waiting for next round");
    }
    else {
        printw("Status: %s",status_name(game->status));
    }

    move((int)game->rows+4,0);

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

    *map_wind=subwin(*main_wind, game->rows+2, game->cols+2, 0, 0);
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

void client_ui_shutdown(WINDOW* main_wind, WINDOW* map_wind)
{
    delwin(map_wind);
    delwin(main_wind);
    endwin();
    refresh();
}
