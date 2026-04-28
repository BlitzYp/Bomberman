#include "../include/client_v2_app.h"

#include <raylib.h>

int main(int argc,char** argv)
{
    client_v2_app_t app;
    (void)argc;
    (void)argv;

    client_v2_app_init(&app);

    InitWindow(app.screen_width,app.screen_height,app.window_title);
    SetTargetFPS(app.target_fps);

    while (!WindowShouldClose()) {
        BeginDrawing();
        client_v2_app_draw(&app);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
