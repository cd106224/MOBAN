#include "sysutils.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common.h"

int tcp_client(uint16_t port) {
  int sock{};
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    ERR_EXIT("tcp client");
  }
  if (port > 0) {
    // 设置地址重复利用
    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on,
                   sizeof(on)) < 0) {
      ERR_EXIT("setsockopt");
    }
    char ip[16];
    get_local_ip(ip);
    sockaddr_in localaddr{};
    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(port);
    localaddr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sock, (struct sockaddr*)&localaddr, sizeof(localaddr)) < 0) {
      ERR_EXIT("bind");
    }
  }
  return sock;
}

int tcp_server(const char* host, uint16_t port) {
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    ERR_EXIT("tcp_server");
  }
  sockaddr_in serveraddr{};
  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  if (host) {
    if (inet_aton(host, &serveraddr.sin_addr) == 0) {
      hostent* hp{};
      if ((hp = gethostbyname(host)) == nullptr) {
        ERR_EXIT("gethostbyname");
      }
      serveraddr.sin_addr = *(in_addr*)(hp->h_addr);
    }
  } else {
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  }
  serveraddr.sin_port = htons(port);
  // 设置地址重复利用
  int on = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on,
                 sizeof(on)) < 0) {
    ERR_EXIT("setsockopt");
  }
  // 绑定地址结构
  if (bind(listenfd, (sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
    ERR_EXIT("bind");
  }
  // 监听
  if (listen(listenfd, SOMAXCONN) < 0) {
    ERR_EXIT("listen");
  }
  return listenfd;
}

int get_local_ip(char* ip) {
  char host[100] = {0};
  if (gethostname(host, sizeof(host)) < 0) {
    return -1;
  }
  struct hostent* hp;
  if ((hp = gethostbyname(host)) == nullptr) {
    return -1;
  }
  strcpy(ip, inet_ntoa(*(struct in_addr*)hp->h_addr));
  return 0;
}

void activate_nonblock(int fd) {
  int ret;
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    ERR_EXIT("fcntl");
  }
  flags |= O_NONBLOCK;
  ret = fcntl(fd, F_SETFL, flags);
  if (ret == -1) {
    ERR_EXIT("fcntl");
  }
}

void deactivate_nonblock(int fd) {
  int ret;
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    ERR_EXIT("fcntl");
  }
  flags &= ~O_NONBLOCK;
  ret = fcntl(fd, F_SETFL, flags);
  if (ret == -1) {
    ERR_EXIT("fcntl");
  }
}

int read_timeout(int fd, uint32_t wait_seconds) {
  int ret = 0;
  if (wait_seconds > 0) {
    fd_set read_fdset;
    timeval timeout{};
    FD_ZERO(&read_fdset);
    FD_SET(fd, &read_fdset);

    timeout.tv_sec = wait_seconds;
    timeout.tv_usec = 0;
    do {
      ret = select(fd + 1, &read_fdset, nullptr, nullptr, &timeout);
    } while (ret < 0 && errno == EINTR);
    if (ret == 0) {
      ret = -1;
      errno = ETIMEDOUT;
    } else if (ret == 1) {
      ret = 0;
    }
  }
  return ret;
}

int write_timeout(int fd, uint32_t wait_seconds) {
  int ret = 0;
  if (wait_seconds > 0) {
    fd_set write_fdset;
    timeval timeout{};
    FD_ZERO(&write_fdset);
    FD_SET(fd, &write_fdset);
    timeout.tv_sec = wait_seconds;
    timeout.tv_usec = 0;
    do {
      ret = select(fd + 1, nullptr, nullptr, &write_fdset, &timeout);
    } while (ret < 0 && errno == EINTR);
    if (ret == 0) {
      ret = -1;
      errno = ETIMEDOUT;
    } else if (ret == 1) {
      ret = 0;
    }
  }
  return ret;
}