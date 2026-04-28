#include "../../shared/include/config.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/net_utils.h"
#include "../include/client_net.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int connect_to_server(const char* ip, int port)
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

int send_hello(int fd, const char* client_id, const char* player_name)
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

int send_player_move(int fd, uint8_t dir)
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

int send_player_bomb(int fd)
{
    msg_bomb_attempt_t attempt;

    attempt.header.msg_type = MSG_BOMB_ATTEMPT;
    attempt.header.sender_id = SERVER_TARGET_ID;
    attempt.header.target_id = SERVER_TARGET_ID;

    return send_header(fd, &attempt.header);
}

int send_set_ready(int fd)
{
    msg_set_ready_t msg;

    msg.header.msg_type=MSG_SET_READY;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=SERVER_TARGET_ID;

    return send_header(fd,&msg.header);
}

int send_select_map_prev(int fd)
{
    msg_select_map_t msg;

    msg.header.msg_type=MSG_SELECT_MAP_PREV;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=SERVER_TARGET_ID;

    return send_header(fd,&msg.header);
}

int send_select_map_next(int fd)
{
    msg_select_map_t msg;

    msg.header.msg_type=MSG_SELECT_MAP_NEXT;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=SERVER_TARGET_ID;

    return send_header(fd,&msg.header);
}

int send_sync_request(int fd)
{
    msg_sync_request_t msg;

    msg.header.msg_type=MSG_SYNC_REQUEST;
    msg.header.sender_id=SERVER_TARGET_ID;
    msg.header.target_id=SERVER_TARGET_ID;

    return send_header(fd,&msg.header);
}

int send_leave(int fd)
{
    msg_header_t header;

    header.msg_type=MSG_LEAVE;
    header.sender_id=SERVER_TARGET_ID;
    header.target_id=SERVER_TARGET_ID;

    return send_header(fd, &header);
}
