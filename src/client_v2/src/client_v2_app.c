#include "../include/client_v2_app.h"

#include <raylib.h>
#include <string.h>
#include <stdio.h>

void client_v2_app_init(client_v2_app_t* app)
{
    if (!app) return;

    memset(app,0,sizeof(*app));
    app->screen_width=1280;
    app->screen_height=720;
    app->target_fps=60;
    snprintf(app->window_title,sizeof(app->window_title),"%s","Bomberman Client V2");
}

void client_v2_app_draw(const client_v2_app_t* app)
{
    if (!app) return;

    ClearBackground(RAYWHITE);
    DrawText(app->window_title,40,32,32,DARKGRAY);
    DrawText("Bomberman game",40,84,20,GREEN);
}
