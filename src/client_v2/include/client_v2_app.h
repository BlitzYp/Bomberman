#ifndef CLIENT_V2_APP_H
#define CLIENT_V2_APP_H

#define FOOTER_STATUS_OFFSET 0
#define FOOTER_DETAIL_OFFSET 28
#define FOOTER_CONTROLS_OFFSET 56
#define FOOTER_EVENT_OFFSET 84

#include "../../shared/include/config.h"
#include "../../client/include/client.h"

#include <raylib.h>

typedef struct {
    int screen_width;
    int screen_height;
    int target_fps;
    char window_title[64];
    int tile_draw_size;
    int board_offset_x;
    int board_offset_y;
    client_game_t game;
    Texture2D floor_texture;
    Texture2D hard_wall_texture;
    Texture2D soft_block_texture;
} client_v2_app_t;

void client_v2_app_init(client_v2_app_t* app);
int client_v2_app_load_assets(client_v2_app_t* app);
void client_v2_app_unload_assets(client_v2_app_t* app);
void client_v2_app_draw(const client_v2_app_t* app);

#endif
