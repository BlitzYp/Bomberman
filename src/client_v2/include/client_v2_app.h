#ifndef CLIENT_V2_APP_H
#define CLIENT_V2_APP_H

#include "../../shared/include/config.h"

typedef struct {
    int screen_width;
    int screen_height;
    int target_fps;
    char window_title[64];
} client_v2_app_t;

void client_v2_app_init(client_v2_app_t* app);
void client_v2_app_draw(const client_v2_app_t* app);

#endif
