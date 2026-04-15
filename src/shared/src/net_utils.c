#include "../include/net_utils.h"
#include "../include/protocol.h"
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

int read_exact(int fd, void* buffer, size_t count)
{
    size_t total=0;
    uint8_t* bytes=(uint8_t *)buffer;
    while (total < count) {
        ssize_t rc=read(fd, bytes + total, count - total);
        if (rc<0) {
            if (errno==EINTR) continue;
            return -1;
        }
        if (rc==0) return -1;
        total+=(size_t)rc;
    }
    return 0;
}

int write_exact(int fd, const void* buffer, size_t count)
{
    size_t total=0;
    const uint8_t* bytes=(const uint8_t *)buffer;

    while (total < count) {
        ssize_t rc=write(fd, bytes + total, count - total);
        if (rc<0) {
            if (errno==EINTR) continue;
            return -1;
        }
        if (rc == 0) return -1;
        total+=(size_t)rc;
    }
    return 0;
}

int send_u8(int fd, uint8_t value)
{
    return write_exact(fd, &value, sizeof(value));
}

int send_u16_be(int fd, uint16_t value)
{
    uint16_t network_value=htons(value);
    return write_exact(fd, &network_value, sizeof(network_value));
}

int recv_u8(int fd, uint8_t* value)
{
    if (value==NULL) return -1;
    return read_exact(fd, value, sizeof(*value));
}

int recv_u16_be(int fd, uint16_t* value)
{
    uint16_t network_value;
    if (value==NULL) return -1;
    if (read_exact(fd, &network_value, sizeof(network_value)) < 0) return -1;
    *value=ntohs(network_value);
    return 0;
}

// Fill the main header
int get_header(int fd,msg_header_t* header)
{
    if (recv_u8(fd,&header->msg_type)<0) return -1;
    if (recv_u8(fd,&header->sender_id)<0) return -1;
    if (recv_u8(fd,&header->target_id)<0) return -1;
    return 0;
}

int send_header(int fd, msg_header_t* header)
{
    if (send_u8(fd,header->msg_type)<0) return -1;
    if (send_u8(fd,header->sender_id)<0) return -1;
    if (send_u8(fd,header->target_id)<0) return -1;
    return 0;
}
