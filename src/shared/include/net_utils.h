#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include "protocol.h"
// be=big endian
int read_exact(int fd, void* buffer, size_t count);
int write_exact(int fd, const void* buffer, size_t count);

int send_u8(int fd, uint8_t value);
int send_u16_be(int fd, uint16_t value);

int recv_u8(int fd, uint8_t* value);
int recv_u16_be(int fd, uint16_t* value);

int get_header(int fd,msg_header_t* header);
int send_header(int fd,msg_header_t* header);
#endif
