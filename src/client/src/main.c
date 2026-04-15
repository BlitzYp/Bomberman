#include "../../shared/include/config.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <ncurses.h>

static int send_header(int fd, const msg_header_t* header)
{
    if (send_u8(fd, header->msg_type) < 0) return -1;
    if (send_u8(fd, header->sender_id) < 0) return -1;
    if (send_u8(fd, header->target_id) < 0) return -1;
    return 0;
}

static int recv_header(int fd, msg_header_t* header)
{
    if (recv_u8(fd, &header->msg_type) < 0) return -1;
    if (recv_u8(fd, &header->sender_id) < 0) return -1;
    if (recv_u8(fd, &header->target_id) < 0) return -1;
    return 0;
}
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

static int recv_welcome(int fd)
{
    msg_header_t header;
    char server_id[MAX_CLIENT_ID_LEN + 1];
    uint8_t status;
    uint8_t player_count;

    if (recv_header(fd, &header) < 0) return -1;
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

static int send_leave(int fd)
{
    msg_header_t header;
    header.msg_type=MSG_LEAVE;
    header.sender_id=SERVER_TARGET_ID;
    header.target_id=SERVER_TARGET_ID;
    return send_header(fd, &header);
}

void draw_map() {
    WINDOW * mainwind, * map_wind, * unbreakable_box;
    initscr();
    int map_width = 30, map_height = 13; // static sizes for testing
    
    if ( (mainwind = initscr()) == NULL ) {
        fprintf(stderr, "Error initialising ncurses.\n");
        // exit(EXIT_FAILURE);
        return;
    }

    noecho();

    map_wind = subwin(mainwind, map_height, map_width, 0, 0);
    box(map_wind, 0, 0);
    mvwaddch(map_wind, 1, 1, 'H');
    // box(map_wind, 2, 2);

    unbreakable_box = subwin(map_wind, 1, 1, 2, 2);
    box(unbreakable_box, 0, 0);

    


    refresh();

    getch();

    // mvwdelch(map_wind, 0, 0);
    wdelch(map_wind);

    wrefresh(map_wind);

    getch();

    delwin(unbreakable_box);
    delwin(map_wind);
    delwin(mainwind);
    endwin();
    refresh();

    return;
}

int main(void)
{
    int fd=connect_to_server("127.0.0.1", 1727);
    if (fd<0) {
        fprintf(stderr, "connect failed\n");
        return 1;
    }

    draw_map();

    if (send_hello(fd, "debug-client", "Janis banis") < 0) {
        fprintf(stderr, "send HELLO failed\n");
        close(fd);
        return 1;
    }

    if (recv_welcome(fd) < 0) {
        fprintf(stderr, "recv WELCOME failed\n");
        close(fd);
        return 1;
    }

    if (send_leave(fd) < 0) {
        fprintf(stderr, "send LEAVE failed\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
