#include "../include/client_v2_app.h"
#include "../include/client_v2_protocol.h"
#include "../../client/include/client_net.h"

#include <errno.h>
#include <poll.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int client_v2_shutdown(client_v2_app_t* app,int fd,int code)
{
    if (app) {
        client_v2_app_unload_assets(app);
        if (IsWindowReady()) CloseWindow();
        free(app->game.tiles);
        free(app->game.bonuses);
    }
    if (fd>=0) close(fd);
    return code;
}

int main(int argc,char** argv)
{
    client_v2_app_t app;
    int fd;
    int dir;
    char client_id[MAX_CLIENT_ID_LEN];
    char player_name[MAX_NAME_LEN];
    const char* host="35.187.169.225";
    int port=1727;
    bool player_name_set=false;

    client_v2_app_init(&app);
    snprintf(client_id,sizeof(client_id),"cv2-%ld",(long)getpid());
    snprintf(player_name,sizeof(player_name),"Player-%ld",(long)getpid());

    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"-l")==0) {
            host="127.0.0.1";
            continue;
        }
        if (!player_name_set) {
            snprintf(player_name,sizeof(player_name),"%s",argv[i]);
            player_name_set=true;
            continue;
        }

        port=atoi(argv[i]);
        if (port<=0 || port>65535) {
            fprintf(stderr,"invalid port\n");
            return 1;
        }
    }

    fd=connect_to_server(host,port);
    if (fd<0) {
        fprintf(stderr,"connect failed\n");
        return 1;
    }
    if (send_hello(fd,client_id,player_name)<0) {
        fprintf(stderr,"send HELLO failed\n");
        close(fd);
        return 1;
    }
    if (client_v2_recv_welcome(fd,&app.game)<0) {
        fprintf(stderr,"recv WELCOME failed\n");
        close(fd);
        return 1;
    }
    if (client_v2_recv_map(fd,&app.game)<0) {
        fprintf(stderr,"recv MAP failed\n");
        close(fd);
        free(app.game.tiles);
        free(app.game.bonuses);
        return 1;
    }

    InitWindow(app.screen_width,app.screen_height,app.window_title);
    SetTargetFPS(app.target_fps);
    if (client_v2_app_load_assets(&app)!=0) {
        fprintf(stderr,"load assets failed\n");
        return client_v2_shutdown(&app,fd,1);
    }

    while (!WindowShouldClose()) {
        for (;;) {
            struct pollfd socket_poll;
            int poll_result;
            int message_result;

            socket_poll.fd=fd;
            socket_poll.events=POLLIN;
            socket_poll.revents=0;
            poll_result=poll(&socket_poll,1,0);
            if (poll_result<0) {
                if (errno==EINTR) continue;
                fprintf(stderr,"poll failed\n");
                return client_v2_shutdown(&app,fd,1);
            }
            if (poll_result==0) break;
            if (socket_poll.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                fprintf(stderr,"server connection closed\n");
                return client_v2_shutdown(&app,fd,1);
            }
            if (!(socket_poll.revents & POLLIN)) break;

            message_result=client_v2_process_server_message(fd,&app.game);
            if (message_result>0) {
                fprintf(stderr,"server disconnected client\n");
                return client_v2_shutdown(&app,fd,0);
            }
            if (message_result<0) {
                fprintf(stderr,"recv server message failed\n");
                return client_v2_shutdown(&app,fd,1);
            }
        }

        dir=-1;
        if (app.game.status==GAME_RUNNING) {
            if (IsKeyPressed(KEY_UP)) dir=DIR_UP;
            else if (IsKeyPressed(KEY_DOWN)) dir=DIR_DOWN;
            else if (IsKeyPressed(KEY_LEFT)) dir=DIR_LEFT;
            else if (IsKeyPressed(KEY_RIGHT)) dir=DIR_RIGHT;

            if (dir!=-1 && send_player_move(fd,(uint8_t)dir)<0) {
                fprintf(stderr,"send move failed\n");
                return client_v2_shutdown(&app,fd,1);
            }

            if (IsKeyPressed(KEY_SPACE) && send_player_bomb(fd)<0) {
                fprintf(stderr,"send bomb failed\n");
                return client_v2_shutdown(&app,fd,1);
            }
        }

        if (IsKeyPressed(KEY_R) && send_set_ready(fd)<0) {
            fprintf(stderr,"send ready failed\n");
            return client_v2_shutdown(&app,fd,1);
        }

        if (app.game.status==GAME_LOBBY) {
            if (IsKeyPressed(KEY_LEFT_BRACKET) && send_select_map_prev(fd)<0) {
                fprintf(stderr,"send map prev failed\n");
                return client_v2_shutdown(&app,fd,1);
            }
            if (IsKeyPressed(KEY_RIGHT_BRACKET) && send_select_map_next(fd)<0) {
                fprintf(stderr,"send map next failed\n");
                return client_v2_shutdown(&app,fd,1);
            }
        }

        BeginDrawing();
        client_v2_app_draw(&app);
        EndDrawing();
    }

    send_leave(fd);
    return client_v2_shutdown(&app,fd,0);
}
