#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "client.h"

#include <ncurses.h>

int client_ui_init(WINDOW** main_wind, WINDOW** map_wind, const client_game_t* game);
void client_ui_shutdown(WINDOW* main_wind, WINDOW* map_wind);
void client_ui_redraw(WINDOW* map_wind, const client_game_t* game);
void client_ui_draw_footer(const client_game_t* game);
void client_ui_set_announcement(client_game_t* game, const char* message);

#endif
