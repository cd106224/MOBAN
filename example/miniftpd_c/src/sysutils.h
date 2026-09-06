#pragma once

#include <cstdint>

int tcp_client(uint16_t port);
int tcp_client(const char* host, uint16_t port);
int get_local_ip(char* ip);
void activate_nonblock(int fd);
void deactivate_nonblock(int fd);
int read_timeout(int fd, uint32_t wait_seconds);
int write_timeout(int fd, uint32_t wait_seconds);