#include "../../shared/include/config.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"
#include "../include/client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>

static int connect_to_server(const char* ip, int port)
{
    int fd=socket(AF_INET, SOCK_STREAM, 0);
    if (fd<0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr)<=0) {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_hello(int fd, const char* client_id, const char* player_name)
{
    msg_header_t header;
    char client_id_buf[MAX_CLIENT_ID_LEN];
    char player_name_buf[MAX_NAME_LEN];

    memset(client_id_buf, 0, sizeof(client_id_buf));
    memset(player_name_buf, 0, sizeof(player_name_buf));

    strncpy(client_id_buf, client_id, MAX_CLIENT_ID_LEN);
    strncpy(player_name_buf, player_name, MAX_NAME_LEN);

    header.msg_type = MSG_HELLO;
    header.sender_id = SERVER_TARGET_ID;
    header.target_id = SERVER_TARGET_ID;

    if (send_header(fd, &header) < 0) return -1;
    if (write_exact(fd, client_id_buf, MAX_CLIENT_ID_LEN) < 0) return -1;
    if (write_exact(fd, player_name_buf, MAX_NAME_LEN) < 0) return -1;

    return 0;
}

static int send_player_move(int fd, uint8_t dir)
{
    msg_move_attempt_t attempt;

    attempt.header.msg_type = MSG_MOVE_ATTEMPT;
    attempt.header.sender_id = SERVER_TARGET_ID;
    attempt.header.target_id = SERVER_TARGET_ID;
    attempt.direction = dir;

    if (send_header(fd, &attempt.header) < 0) return -1;
    if (send_u8(fd, attempt.direction) < 0) return -1;

    return 0;
}

static int send_player_bomb(int fd)
{
    msg_bomb_attempt_t attempt;

    attempt.header.msg_type = MSG_BOMB_ATTEMPT;
    attempt.header.sender_id = SERVER_TARGET_ID;
    attempt.header.target_id = SERVER_TARGET_ID;

    if (send_header(fd, &attempt.header) < 0) return -1;

    return 0;
}

static int recv_welcome(int fd)
{
    msg_header_t header;
    char server_id[MAX_CLIENT_ID_LEN + 1];
    uint8_t status;
    uint8_t player_count;

    if (get_header(fd, &header) < 0) return -1;
    if (header.msg_type != MSG_WELCOME) return -1;

    if (read_exact(fd, server_id, MAX_CLIENT_ID_LEN) < 0) return -1;
    server_id[MAX_CLIENT_ID_LEN] = '\0';

    if (recv_u8(fd, &status) < 0) return -1;
    if (recv_u8(fd, &player_count) < 0) return -1;

    printf("WELCOME\n");
    printf("assigned_id=%u\n", header.sender_id);
    printf("server_id=%s\n", server_id);
    printf("status=%u\n", status);
    printf("player_count=%u\n", player_count);

    for (uint8_t i=0;i<player_count; i++) {
        uint8_t id;
        uint8_t ready;
        char name[MAX_NAME_LEN + 1];

        if (recv_u8(fd, &id) < 0) return -1;
        if (recv_u8(fd, &ready) < 0) return -1;
        if (read_exact(fd, name, MAX_NAME_LEN) < 0) return -1;
        name[MAX_NAME_LEN] = '\0';

        printf("player[%u]: id=%u ready=%u name=%s\n", i, id, ready, name);
    }

    return 0;
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

    wrefresh(map_wind);
}

static void redraw_game(WINDOW* win, const client_game_t* game)
{
    draw_map(win,game);

    for (uint8_t i=0;i<MAX_PLAYERS;i++) {
        if (!game->players[i].known) continue;
        mvwaddch(win,game->players[i].row+1,game->players[i].col+1,(chtype)('1'+i));
    }

    wrefresh(win);
}

static int recv_map_payload(int fd, client_game_t* game)
{
    uint16_t rows;
    uint16_t cols;

    if (recv_u16_be(fd, &rows)<0) return -1;
    if (recv_u16_be(fd, &cols)<0) return -1;
    if (rows==0 || cols==0) return -1;

    tile_t* tiles=malloc((size_t)rows*cols*sizeof(*tiles));
    if (!tiles) return -1;

    for (uint32_t i=0;i<(uint32_t)rows*cols;i++) {
        uint8_t tile;
        if (recv_u8(fd, &tile)<0) {
            free(tiles);
            return -1;
        }

        if (tile>TILE_SOFT_BLOCK) {
            free(tiles);
            return -1;
        }

        tiles[i]=(tile_t)tile;
    }

    free(game->tiles);
    game->rows=rows;
    game->cols=cols;
    game->tiles=tiles;

    return 0;
}

static int recv_map(int fd, client_game_t* game)
{
    msg_header_t header;

    if (get_header(fd, &header)<0) return -1;
    if (header.msg_type!=MSG_MAP) return -1;

    return recv_map_payload(fd,game);
}

static int recv_player_move_payload(int fd, WINDOW* win, client_game_t* game)
{
    uint8_t player_id;
    uint16_t cell_index;
    uint16_t row;
    uint16_t col;

    if (recv_u8(fd, &player_id) < 0) return -1;
    if (recv_u16_be(fd, &cell_index) < 0) return -1;
    if (player_id>=MAX_PLAYERS || game->cols==0) return -1;

    row=cell_index/game->cols;
    col=cell_index%game->cols;
    if (row>=game->rows || col>=game->cols) return -1;

    game->players[player_id].known=true;
    game->players[player_id].row=row;
    game->players[player_id].col=col;

    return 0;
}

static int recv_player_bomb_payload(int fd, WINDOW* win, client_game_t* game)
{
    uint8_t player_id;
    uint16_t cell_index;
    uint16_t row;
    uint16_t col;

    if (recv_u8(fd, &player_id) < 0) return -1;
    if (recv_u16_be(fd, &cell_index) < 0) return -1;
    if (player_id>=MAX_PLAYERS || game->cols==0) return -1;

    row=cell_index/game->cols;
    col=cell_index%game->cols;
    if (row>=game->rows || col>=game->cols) return -1;

    game->tiles[cell_index]=TILE_BOMB;
    // game->tiles[player_id].known=true;
    // game->tiles[player_id].row=row;
    // game->players[player_id].col=col;

    return 0;
}

static int recv_bomb_start_payload(int fd, WINDOW* win, client_game_t* game)
{
    uint8_t radius;
    uint16_t cell_index;
    uint16_t row;
    uint16_t col;

    if (recv_u8(fd, &radius) < 0) return -1;
    if (recv_u16_be(fd, &cell_index) < 0) return -1;
    // if (player_id>=MAX_PLAYERS || game->cols==0) return -1;

    row=cell_index/game->cols;
    col=cell_index%game->cols;
    if (row>=game->rows || col>=game->cols) return -1;

    game->tiles[cell_index]=TILE_BOMB_EXPLODE;
    int index=0;
    for (int i = 1; i < radius+1; i++) {
        index=cell_index+i;
        if (cell_index/game->cols != index/game->cols) break;
        if (game->tiles[index]!=TILE_HARD_WALL) {
            game->tiles[index]=TILE_BOMB_EXPLODE;    
        } else if (game->tiles[index]==TILE_HARD_WALL) {
            break;
        }
    }
    for (int i = 1; i < radius+1; i++) {
        index=cell_index+i*game->cols;
        if (cell_index%game->cols != index%game->cols) break;
        if (game->tiles[index]!=TILE_HARD_WALL) {
            game->tiles[index]=TILE_BOMB_EXPLODE;    
        } else if (game->tiles[index]==TILE_HARD_WALL) {
            break;    
        }
    }
    for (int i = -1; i > (-1*(radius+1)); i--) {
        index=cell_index+i;
        if (cell_index/game->cols != index/game->cols) break;
        if (game->tiles[index]!=TILE_HARD_WALL) {
            game->tiles[index]=TILE_BOMB_EXPLODE;    
        } else if (game->tiles[index]==TILE_HARD_WALL) {
            break;    
        }
    }
    for (int i = -1; i > (-1*(radius+1)); i--) {
        index=cell_index+i*game->cols;
        if (cell_index%game->cols != index%game->cols) break;
        if (game->tiles[index]!=TILE_HARD_WALL) {
            game->tiles[index]=TILE_BOMB_EXPLODE;    
        } else if (game->tiles[index]==TILE_HARD_WALL) {
            break;
        }
    }
    // printf("index %d\n", cell_index);
    // printf("rad %d\n", -1*(radius+1));
    // printf("rad %d\n", radius);
    // game->tiles[player_id].known=true;
    // game->tiles[player_id].row=row;
    // game->players[player_id].col=col;

    return 0;
}

static int recv_bomb_end_payload(int fd, WINDOW* win, client_game_t* game)
{
    uint16_t cell_index;
    uint16_t row;
    uint16_t col;

    if (recv_u16_be(fd, &cell_index) < 0) return -1;
    // if (player_id>=MAX_PLAYERS || game->cols==0) return -1;

    row=cell_index/game->cols;
    col=cell_index%game->cols;
    if (row>=game->rows || col>=game->cols) return -1;

    game->tiles[cell_index]=TILE_EMPTY;
    int index=1;
    while(true)
    {
        if (game->tiles[cell_index+index]==TILE_BOMB_EXPLODE) {
            game->tiles[cell_index+index]=TILE_EMPTY;
            index++;
        }
        if (game->tiles[cell_index+index]!=TILE_BOMB_EXPLODE) break;
    }
    index=-1;
    while(true)
    {
        if (game->tiles[cell_index+index]==TILE_BOMB_EXPLODE) {
            game->tiles[cell_index+index]=TILE_EMPTY;
            index--;
        }
        if (game->tiles[cell_index+index]!=TILE_BOMB_EXPLODE) break;
    }
    index=1;
    while(true)
    {
        if (game->tiles[cell_index+index*game->cols]==TILE_BOMB_EXPLODE) {
            game->tiles[cell_index+index*game->cols]=TILE_EMPTY;
            index++;
        }
        if (game->tiles[cell_index+index*game->cols]!=TILE_BOMB_EXPLODE) break;
    }
    index=-1;
    while(true)
    {
        if (game->tiles[cell_index+index*game->cols]==TILE_BOMB_EXPLODE) {
            game->tiles[cell_index+index*game->cols]=TILE_EMPTY;
            index--;
        }
        if (game->tiles[cell_index+index*game->cols]!=TILE_BOMB_EXPLODE) break;
    }

    return 0;
}

// PROCESS SERVER EVENTS HERE
static int process_server_message(int fd, WINDOW* win, client_game_t* game)
{
    msg_header_t header;
    if (get_header(fd, &header)<0) return -1;
    switch (header.msg_type) {
        case MSG_MAP:
            if (recv_map_payload(fd,game)<0) return -1;
            redraw_game(win,game);
            return 0;
        case MSG_MOVED:
            if (recv_player_move_payload(fd,win,game)!=0) return -1;
            redraw_game(win,game);
            return 0;
        case MSG_BOMB:
            if (recv_player_bomb_payload(fd,win,game)!=0) return -1;
            redraw_game(win,game);
            return 0;
        case MSG_EXPLOSION_START:
            if (recv_bomb_start_payload(fd,win,game)!=0) return -1;
            redraw_game(win,game);
            return 0;
        case MSG_EXPLOSION_END:
            if (recv_bomb_end_payload(fd,win,game)!=0) return -1;
            redraw_game(win,game);
            return 0;
        default: return -1;
    }
}

static int send_leave(int fd)
{
    msg_header_t header;
    header.msg_type=MSG_LEAVE;
    header.sender_id=SERVER_TARGET_ID;
    header.target_id=SERVER_TARGET_ID;
    return send_header(fd, &header);
}

void end_wind(WINDOW* mainwind, WINDOW* map_wind) {
    delwin(map_wind);
    delwin(mainwind);
    endwin();
    refresh();
    return;
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
        fprintf(stderr, "connect failed\n");
        return 1;
    }

    if (send_hello(fd, client_id, player_name) < 0) {
        fprintf(stderr, "send HELLO failed\n");
        close(fd);
        return 1;
    }

    if (recv_welcome(fd) < 0) {
        fprintf(stderr, "recv WELCOME failed\n");
        close(fd);
        return 1;
    }

    if (recv_map(fd,&game) < 0) {
        fprintf(stderr, "recv MAP failed\n");
        close(fd);
        free(game.tiles);
        return 1;
    }

    WINDOW* mainwind;
    WINDOW* map_wind;
    int ch = 0, dir = -1;
    if ((mainwind = initscr()) == NULL) {
        fprintf(stderr, "Error initialising ncurses.\n");
        close(fd);
        free(game.tiles);
        return 1;
    }

    map_wind = subwin(mainwind, game.rows+2, game.cols+2, 0, 0);
    if (!map_wind) {
        endwin();
        fprintf(stderr, "Error creating map window.\n");
        close(fd);
        free(game.tiles);
        return 1;
    }
    noecho();
    curs_set(0);
    keypad(mainwind, TRUE);
    nodelay(mainwind, TRUE);
    redraw_game(map_wind,&game);

    // Check continously if the server has sent something, if yes, then we render
    while(1) {
        struct pollfd socket_poll;
        socket_poll.fd=fd;
        socket_poll.events=POLLIN;
        socket_poll.revents=0;

        int poll_result=poll(&socket_poll,1,50);
        if (poll_result<0) {
            if (errno==EINTR) continue;
            end_wind(mainwind, map_wind);
            fprintf(stderr, "poll failed\n");
            close(fd);
            free(game.tiles);
            return 1;
        }

        if (poll_result>0) {
            if (socket_poll.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                end_wind(mainwind, map_wind);
                fprintf(stderr, "server connection closed\n");
                close(fd);
                free(game.tiles);
                return 1;
            }

            if (socket_poll.revents & POLLIN) {
                if (process_server_message(fd, map_wind, &game) < 0) {
                    end_wind(mainwind, map_wind);
                    fprintf(stderr, "recv server message failed\n");
                    close(fd);
                    free(game.tiles);
                    return 1;
                }
            }
        }

        dir=-1;
        ch = getch();
        if (ch==ERR) continue;

        switch (ch) {
            case KEY_UP:
                dir = DIR_UP;
                break;
            case KEY_DOWN:
                dir = DIR_DOWN;
                break;
            case KEY_LEFT:
                dir = DIR_LEFT;
                break;
            case KEY_RIGHT:
                dir = DIR_RIGHT;
                break;
            case ' ':
                dir = -1;
                if (send_player_bomb(fd) < 0) {
                    end_wind(mainwind, map_wind);
                    fprintf(stderr, "send bomb failed\n");
                    close(fd);
                    free(game.tiles);
                    return 1;
                }
                break;
            case 'q':
                send_leave(fd);
                end_wind(mainwind, map_wind);
                close(fd);
                free(game.tiles);
                return 0;
            default:
                continue;
        }

        if (dir!=-1) {
            if (send_player_move(fd, (uint8_t)dir) < 0) {
                end_wind(mainwind, map_wind);
                fprintf(stderr, "send move failed\n");
                close(fd);
                free(game.tiles);
                return 1;
            }
        }    
    }

    end_wind(mainwind, map_wind);

    if (send_leave(fd) < 0) {
        fprintf(stderr, "send LEAVE failed\n");
        close(fd);
        free(game.tiles);
        return 1;
    }

    close(fd);
    free(game.tiles);
    return 0;
}
