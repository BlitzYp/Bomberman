#include "../include/client_v2_app.h"

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const Color PLAYER_COLORS[MAX_PLAYERS]={
    RED,
    SKYBLUE,
    LIME,
    GOLD,
    PURPLE,
    ORANGE,
    BLUE,
    PINK
};

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

static const char* player_name_or_unknown(const client_game_t* game,uint8_t player_id)
{
    if (!game || player_id>=MAX_PLAYERS) return "Unknown";
    if (game->players[player_id].name[0]=='\0') return "Unknown";
    return game->players[player_id].name;
}

void client_v2_app_init(client_v2_app_t* app)
{
    if (!app) return;

    memset(app,0,sizeof(*app));
    app->screen_width=1024;
    app->screen_height=800;
    app->target_fps=60;
    app->tile_draw_size=48;
    app->board_offset_x=48;
    app->board_offset_y=96;
    snprintf(app->window_title,sizeof(app->window_title),"%s","Bomberman");
}

int client_v2_app_load_assets(client_v2_app_t* app)
{
    if (!app) return -1;

    app->floor_texture=LoadTexture("src/assets/gfx/tiles/floor.png");
    app->hard_wall_texture=LoadTexture("src/assets/gfx/tiles/hard_wall.png");
    app->soft_block_texture=LoadTexture("src/assets/gfx/tiles/soft_block.png");

    if (app->floor_texture.id==0 || app->hard_wall_texture.id==0 || app->soft_block_texture.id==0) {
        client_v2_app_unload_assets(app);
        return -1;
    }

    return 0;
}

void client_v2_app_unload_assets(client_v2_app_t* app)
{
    if (!app) return;

    if (app->floor_texture.id!=0) UnloadTexture(app->floor_texture);
    if (app->hard_wall_texture.id!=0) UnloadTexture(app->hard_wall_texture);
    if (app->soft_block_texture.id!=0) UnloadTexture(app->soft_block_texture);
    app->floor_texture.id=0;
    app->hard_wall_texture.id=0;
    app->soft_block_texture.id=0;
}

static Texture2D tile_texture(const client_v2_app_t* app,tile_t tile)
{
    switch (tile) {
        case TILE_HARD_WALL:
            return app->hard_wall_texture;
        case TILE_SOFT_BLOCK:
            return app->soft_block_texture;
        case TILE_EMPTY:
        case TILE_BOMB:
        case TILE_BOMB_EXPLODE:
        default:
            return app->floor_texture;
    }
}

static Rectangle board_cell_rect(const client_v2_app_t* app,uint16_t row,uint16_t col)
{
    Rectangle rect;
    rect.x=(float)(app->board_offset_x+col*app->tile_draw_size);
    rect.y=(float)(app->board_offset_y+row*app->tile_draw_size);
    rect.width=(float)app->tile_draw_size;
    rect.height=(float)app->tile_draw_size;

    return rect;
}

void client_v2_app_draw(const client_v2_app_t* app)
{
    Rectangle src;
    char status_line[96];
    char detail_line[160];
    char controls_line[128];
    int footer_y;

    if (!app) return;

    ClearBackground(RAYWHITE);

    if (!app->game.tiles || app->game.rows==0 || app->game.cols==0) {
        DrawText(app->window_title,40,32,32,DARKGRAY);
        DrawText("No map loaded",40,120,24,RED);
        return;
    }

    src.x=0.0f;
    src.y=0.0f;
    src.width=16.0f;
    src.height=16.0f;

    for (uint16_t row=0;row<app->game.rows;row++) {
        for (uint16_t col=0;col<app->game.cols;col++) {
            uint16_t cell=make_cell_index(row,col,app->game.cols);
            Texture2D texture=tile_texture(app,app->game.tiles[cell]);
            Rectangle dst=board_cell_rect(app,row,col);

            DrawTexturePro(texture,src,dst,(Vector2){0.0f,0.0f},0.0f,WHITE);
            // Bombs
            if (app->game.tiles[cell]==TILE_BOMB) {
                DrawCircle((int)(dst.x+dst.width*0.5f),(int)(dst.y+dst.height*0.5f),(float)(app->tile_draw_size*0.22f),DARKGRAY);
                DrawCircle((int)(dst.x+dst.width*0.42f),(int)(dst.y+dst.height*0.42f),(float)(app->tile_draw_size*0.06f),RAYWHITE);
            }
            // Explosions
            else if (app->game.tiles[cell]==TILE_BOMB_EXPLODE) {
                DrawRectangleRec(dst,Fade(ORANGE,0.70f));
                DrawRectangleLinesEx(dst,2.0f,GOLD);
            }
            // Bonuses
            if (app->game.bonuses && app->game.bonuses[cell]!=BONUS_NONE && app->game.tiles[cell]==TILE_EMPTY) {
                if (app->game.bonuses[cell]==BONUS_BOMB_COUNT) {
                    DrawCircleGradient((Vector2){ dst.x+dst.width*0.5f, dst.y+dst.height*0.5f }, (float)(app->tile_draw_size*0.3f), GRAY, RED);
                    DrawText("N", dst.x+dst.width*0.4f, dst.y+dst.height*0.35f, 18, BLACK);
                }
                if (app->game.bonuses[cell]==BONUS_RADIUS) {
                    DrawCircleGradient((Vector2){ dst.x+dst.width*0.5f, dst.y+dst.height*0.5f }, (float)(app->tile_draw_size*0.3f), GRAY, YELLOW);
                    DrawText("R", dst.x+dst.width*0.4f, dst.y+dst.height*0.35f, 18, BLACK);
                }
                if (app->game.bonuses[cell]==BONUS_SPEED) { 
                    DrawCircleGradient((Vector2){ dst.x+dst.width*0.5f, dst.y+dst.height*0.5f }, (float)(app->tile_draw_size*0.3f), RED, YELLOW);
                    DrawText("A", dst.x+dst.width*0.4f, dst.y+dst.height*0.35f, 18, BLACK);
                }            
                if (app->game.bonuses[cell]==BONUS_TIMER) { 
                    DrawCircleGradient((Vector2){ dst.x+dst.width*0.5f, dst.y+dst.height*0.5f }, (float)(app->tile_draw_size*0.3f), GRAY, GREEN);
                    DrawText("T", dst.x+dst.width*0.38f, dst.y+dst.height*0.35f, 18, BLACK);
                }
            }
        }
    }

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!app->game.players[i].known || !app->game.players[i].alive) continue;
        if (app->game.players[i].row>=app->game.rows || app->game.players[i].col>=app->game.cols) continue;

        Rectangle dst=board_cell_rect(app,app->game.players[i].row,app->game.players[i].col);
        float size=(float)app->tile_draw_size;

        DrawCircle((int)(dst.x+size*0.5f),(int)(dst.y+size*0.5f),size*0.24f,PLAYER_COLORS[i]);
        DrawText(TextFormat("%u",(unsigned)(i+1)),(int)(dst.x+size*0.4f),(int)(dst.y+size*0.35f),18,BLACK);
    }

    snprintf(status_line,sizeof(status_line),"Status: %s",status_name(app->game.status));
    detail_line[0]='\0';
    controls_line[0]='\0';

    if (app->game.has_winner) {
        snprintf(detail_line,sizeof(detail_line),"Winner: %s",player_name_or_unknown(&app->game,app->game.winner_id));
        snprintf(controls_line,sizeof(controls_line),"Press R to return to lobby | ESC quit");
    } else if (app->game.status==GAME_LOBBY) {
        if (app->game.selected_map_name[0]!='\0') snprintf(detail_line,sizeof(detail_line),"Map: %s | %u rows x %u cols | players 2-8",app->game.selected_map_name,app->game.rows,app->game.cols);
        else snprintf(detail_line,sizeof(detail_line),"Map: (unknown)");
        snprintf(controls_line,sizeof(controls_line),"Lobby: [ ] change map | R ready | ESC quit");
    } else if (app->game.status==GAME_END) {
        snprintf(detail_line,sizeof(detail_line),"Game over");
        snprintf(controls_line,sizeof(controls_line),"Press R to restart | ESC quit");
    } else if (app->game.status==GAME_RUNNING && app->game.waiting_for_next_round) {
        snprintf(detail_line,sizeof(detail_line),"Waiting for next round");
        snprintf(controls_line,sizeof(controls_line),"ESC quit");
    } else {
        snprintf(controls_line,sizeof(controls_line),"Running: arrows move | SPACE bomb | ESC quit");
    }

    footer_y=app->board_offset_y+app->game.rows*app->tile_draw_size+24;
    DrawText(status_line,40,footer_y,24,DARKGRAY);
    if (detail_line[0]!='\0') DrawText(detail_line,40,footer_y+28,20,GRAY);
    if (controls_line[0]!='\0') DrawText(controls_line,40,footer_y+54,20,GRAY);
}
