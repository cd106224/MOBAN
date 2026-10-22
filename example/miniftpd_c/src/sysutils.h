#pragma once

#include <netinet/in.h>

#include <cstdint>
#include <string>

int tcp_client(uint16_t port);
int tcp_client(const char* host, uint16_t port);
int get_local_ip(char* ip);
void activate_nonblock(int fd);
void deactivate_nonblock(int fd);
int read_timeout(int fd, uint32_t wait_seconds);
int write_timeout(int fd, uint32_t wait_seconds);
int accept_timeout(int fd, sockaddr_in* addr, uint32_t wait_seconds);
int connect_timeout(int fd, sockaddr_in* addr, uint32_t wait_seconds);
ssize_t readn(int fd, void* buf, size_t count);
ssize_t writen(int fd, const void* buf, size_t count);
ssize_t recv_peek(int sockfd, void* buf, size_t len);
ssize_t readline(int sockfd, void* buf, size_t maxline);
void send_fd(int sock_fd, int fd);
int recv_fd(const int sock_fd);
std::string statbuf_get_perms(const struct stat* sbuf);
std::string statbuf_get_date(const struct stat* sbuf);
int lock_internal(int fd, int lock_type);
int lock_file_read(int fd);
int lock_file_write(int fd);
int unlock_file(int fd);
int64_t get_time_sec();
int64_t get_time_usec();
void activate_oobinline(int fd);
void activate_sigurg(int fd);