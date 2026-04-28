#include "../include/client.h"
#include "../include/client_net.h"
#include "../include/client_protocol.h"
#include "../include/client_ui.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

static void client_game_cleanup(client_game_t* game)
{
    free(game->tiles);
    free(game->bonuses);
}

static int client_fail(WINDOW* main_wind, WINDOW* map_wind, int fd, client_game_t* game, const char* message)
{
    if (map_wind || main_wind) client_ui_shutdown(main_wind,map_wind);
    if (message) fprintf(stderr,"%s\n",message);
    if (fd>=0) close(fd);
    client_game_cleanup(game);
    return 1;
}

int main(int argc, char** argv)
{
    char client_id[MAX_CLIENT_ID_LEN];
    char player_name[MAX_NAME_LEN];

    snprintf(client_id,sizeof(client_id),"client-%ld",(long)getpid());
    if (argc>1) snprintf(player_name,sizeof(player_name),"%s",argv[1]);
    else snprintf(player_name,sizeof(player_name),"Player-%ld",(long)getpid());

    client_game_t game;
    memset(&game,0,sizeof(game));

    int fd=connect_to_server("127.0.0.1", 1727);
    if (fd<0) {
        return client_fail(NULL,NULL,-1,&game,"connect failed");
    }

    if (send_hello(fd, client_id, player_name) < 0) {
        return client_fail(NULL,NULL,fd,&game,"send HELLO failed");
    }

    if (recv_welcome(fd,&game) < 0) {
        return client_fail(NULL,NULL,fd,&game,"recv WELCOME failed");
    }

    if (recv_map(fd,&game) < 0) {
        return client_fail(NULL,NULL,fd,&game,"recv MAP failed");
    }

    WINDOW* mainwind;
    WINDOW* map_wind;
    int ch = 0, dir = -1;
    mainwind=NULL;
    map_wind=NULL;
    if (client_ui_init(&mainwind,&map_wind,&game)!=0) {
        return client_fail(NULL,NULL,fd,&game,"Error initialising ncurses.");
    }
    client_ui_redraw(map_wind,&game);

    // Check continously if the server has sent something, if yes, then we render
    while(1) {
        struct pollfd socket_poll;
        socket_poll.fd=fd;
        socket_poll.events=POLLIN;
        socket_poll.revents=0;

        int poll_result=poll(&socket_poll,1,50);
        if (poll_result<0) {
            if (errno==EINTR) continue;
            return client_fail(mainwind,map_wind,fd,&game,"poll failed");
        }

        if (poll_result>0) {
            if (socket_poll.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return client_fail(mainwind,map_wind,fd,&game,"server connection closed");
            }

            if (socket_poll.revents & POLLIN) {
                int message_result=process_server_message(fd, &map_wind, &game);
                if (message_result>0) {
                    client_ui_shutdown(mainwind,map_wind);
                    close(fd);
                    client_game_cleanup(&game);
                    return 0;
                }
                if (message_result<0) {
                    return client_fail(mainwind,map_wind,fd,&game,"recv server message failed");
                }
            }
        }

        dir=-1;
        ch = getch();
        if (ch==ERR) continue;

        switch (ch) {
            case KEY_UP:
                if (game.status!=GAME_RUNNING) continue;
                dir = DIR_UP;
                break;
            case KEY_DOWN:
                if (game.status!=GAME_RUNNING) continue;
                dir = DIR_DOWN;
                break;
            case KEY_LEFT:
                if (game.status!=GAME_RUNNING) continue;
                dir = DIR_LEFT;
                break;
            case KEY_RIGHT:
                if (game.status!=GAME_RUNNING) continue;
                dir = DIR_RIGHT;
                break;
            case ' ':
                if (game.status!=GAME_RUNNING) continue;
                dir = -1;
                if (send_player_bomb(fd) < 0) {
                    return client_fail(mainwind,map_wind,fd,&game,"send bomb failed");
                }
                break;
            case 'r':
                dir=-1;
                if (send_set_ready(fd) < 0) {
                    return client_fail(mainwind,map_wind,fd,&game,"send ready failed");
                }
                break;
            case '[':
                if (game.status!=GAME_LOBBY) continue;
                dir=-1;
                if (send_select_map_prev(fd) < 0) {
                    return client_fail(mainwind,map_wind,fd,&game,"send map prev failed");
                }
                break;
            case ']':
                if (game.status!=GAME_LOBBY) continue;
                dir=-1;
                if (send_select_map_next(fd) < 0) {
                    return client_fail(mainwind,map_wind,fd,&game,"send map next failed");
                }
                break;
            case 'q':
                send_leave(fd);
                client_ui_shutdown(mainwind,map_wind);
                close(fd);
                client_game_cleanup(&game);
                return 0;
            default:
                continue;
        }

        if (dir!=-1) {
            if (send_player_move(fd, (uint8_t)dir) < 0) {
                return client_fail(mainwind,map_wind,fd,&game,"send move failed");
            }
        }
    }

    client_ui_shutdown(mainwind,map_wind);

    if (send_leave(fd) < 0) {
        return client_fail(NULL,NULL,fd,&game,"send LEAVE failed");
    }

    close(fd);
    client_game_cleanup(&game);
    return 0;
}
