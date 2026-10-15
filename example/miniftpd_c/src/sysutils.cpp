#include "sysutils.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>

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

int accept_timeout(int fd, sockaddr_in* addr, uint32_t wait_seconds) {
  int ret;
  socklen_t addrlen = sizeof(sockaddr_in);
  if (wait_seconds > 0) {
    fd_set accept_fdset;
    timeval timeout{};
    FD_ZERO(&accept_fdset);
    FD_SET(fd, &accept_fdset);
    timeout.tv_sec = wait_seconds;
    timeout.tv_usec = 0;
    do {
      ret = select(fd + 1, &accept_fdset, nullptr, nullptr, &timeout);
    } while (ret < 0 && errno == EINTR);
    if (ret == -1) {
      return -1;
    } else if (ret == 0) {
      errno = ETIMEDOUT;
      return -1;
    }
  }
  if (addr != nullptr) {
    ret = accept(fd, (sockaddr*)addr, &addrlen);
  } else {
    ret = accept(fd, nullptr, nullptr);
  }
  return ret;
}

int connect_timeout(int fd, sockaddr_in* addr, uint32_t wait_seconds) {
  int ret;
  socklen_t addrlen = sizeof(sockaddr_in);
  if (wait_seconds > 0) {
    activate_nonblock(fd);
  }
  ret = connect(fd, (struct sockaddr*)addr, addrlen);
  if (ret < 0 && errno == EINPROGRESS) {
    fd_set connect_fdset;
    timeval timeout{};
    FD_ZERO(&connect_fdset);
    FD_SET(fd, &connect_fdset);
    timeout.tv_sec = wait_seconds;
    timeout.tv_usec = 0;
    do {
      ret = select(fd + 1, nullptr, &connect_fdset, nullptr, &timeout);
    } while (ret < 0 && errno == EINTR);
  }
  if (ret == 0) {
    ret = -1;
    errno = ETIMEDOUT;
  } else if (ret < 0) {
    return -1;
  } else if (ret == 1) {
    /* ret返回为1，可能有两种情况，一种是连接建立成功，一种是套接字产生错误，*/
    /* 此时错误信息不会保存至errno变量中，因此，需要调用getsockopt来获取。 */
    int err;
    socklen_t socklen = sizeof(err);
    int sockoptret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &socklen);
    if (sockoptret == -1) {
      return -1;
    }
    if (err == 0) {
      ret = 0;
    } else {
      errno = err;
      ret = -1;
    }
  }
  if (wait_seconds > 0) {
    deactivate_nonblock(fd);
  }
  return ret;
}

ssize_t readn(int fd, void* buf, size_t count) {
  size_t nleft = count;
  ssize_t nread;
  char* bufp = (char*)buf;

  while (nleft > 0) {
    if ((nread = read(fd, bufp, nleft)) < 0) {
      if (errno == EINTR) continue;
      return -1;
    } else if (nread == 0)
      // 对方关闭连接
      return count - nleft;

    bufp += nread;
    nleft -= nread;
  }

  return count;
}

ssize_t writen(int fd, const void* buf, size_t count) {
  size_t nleft = count;
  ssize_t nwritten;
  char* bufp = (char*)buf;

  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    } else if (nwritten == 0) {
      return -1;  // count > 0 却什么都没写，异常
    }
    bufp += nwritten;
    nleft -= nwritten;
  }

  return static_cast<ssize_t>(count);
}

ssize_t recv_peek(int sockfd, void* buf, size_t len) {
  while (1) {
    int ret = recv(sockfd, buf, len, MSG_PEEK);
    if (ret == -1 && errno == EINTR) continue;
    return ret;
  }
}

ssize_t readline(int sockfd, void* buf, size_t maxline) {
  size_t nleft = maxline;
  ssize_t nread;
  char* bufp = static_cast<char*>(buf);

  while (1) {
    nread = recv_peek(sockfd, bufp, nleft);
    if (nread < 0) {
      return nread;
    } else if (nread == 0) {
      return 0;
    }

    for (size_t i = 0; i < static_cast<size_t>(nread); ++i) {
      if (bufp[i] == '\n') {
        ssize_t nline = readn(sockfd, bufp, i + 1);
        if (nline != static_cast<ssize_t>(i + 1)) {
          exit(EXIT_FAILURE);
        }
        return nline;
      }
    }

    // 窥到的数据填满了剩余空间却仍没有 '\n'，说明整行比 maxline 还长
    if (static_cast<size_t>(nread) >= nleft) {
      exit(EXIT_FAILURE);
    }
    nleft -= nread;
    if (readn(sockfd, bufp, nread) != nread) {
      exit(EXIT_FAILURE);
    }
    bufp += nread;
  }
}

// 把一个已经打开的文件描述符传给另一个进程
void send_fd(int sock_fd, int fd) {
  int ret;
  struct msghdr msg;
  struct cmsghdr* p_cmsg;
  struct iovec vec;
  char cmsgbuf[CMSG_SPACE(sizeof(fd))];
  int* p_fds;
  char sendchar = 0;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);
  p_cmsg = CMSG_FIRSTHDR(&msg);
  p_cmsg->cmsg_level = SOL_SOCKET;
  p_cmsg->cmsg_type = SCM_RIGHTS;
  p_cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
  p_fds = (int*)CMSG_DATA(p_cmsg);
  *p_fds = fd;

  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &vec;
  msg.msg_iovlen = 1;
  msg.msg_flags = 0;

  vec.iov_base = &sendchar;
  vec.iov_len = sizeof(sendchar);
  ret = sendmsg(sock_fd, &msg, 0);
  if (ret != 1) ERR_EXIT("sendmsg");
}

int recv_fd(const int sock_fd) {
  int ret;
  struct msghdr msg;
  char recvchar;
  struct iovec vec;
  int recv_fd;
  char cmsgbuf[CMSG_SPACE(sizeof(recv_fd))];
  struct cmsghdr* p_cmsg;
  int* p_fd;
  vec.iov_base = &recvchar;
  vec.iov_len = sizeof(recvchar);
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &vec;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);
  msg.msg_flags = 0;

  p_fd = (int*)CMSG_DATA(CMSG_FIRSTHDR(&msg));
  *p_fd = -1;
  ret = recvmsg(sock_fd, &msg, 0);
  if (ret != 1) ERR_EXIT("recvmsg");

  p_cmsg = CMSG_FIRSTHDR(&msg);
  if (p_cmsg == nullptr) ERR_EXIT("no passed fd");

  p_fd = (int*)CMSG_DATA(p_cmsg);
  recv_fd = *p_fd;
  if (recv_fd == -1) ERR_EXIT("no passed fd");

  return recv_fd;
}

std::string statbuf_get_perms(const struct stat* sbuf) {
  std::string perms = "----------";
  mode_t mode = sbuf->st_mode;
  switch (mode & S_IFMT) {
    case S_IFREG:
      perms[0] = '-';
      break;
    case S_IFDIR:
      perms[0] = 'd';
      break;
    case S_IFLNK:
      perms[0] = 'l';
      break;
    case S_IFIFO:
      perms[0] = 'p';
      break;
    case S_IFSOCK:
      perms[0] = 's';
      break;
    case S_IFCHR:
      perms[0] = 'c';
      break;
    case S_IFBLK:
      perms[0] = 'b';
      break;
    default:
      perms[0] = '?';
      break;
  }

  // 权限位
  // 获取文件9个权限位
  if (mode & S_IRUSR) {
    perms[1] = 'r';
  }
  if (mode & S_IWUSR) {
    perms[2] = 'w';
  }
  if (mode & S_IXUSR) {
    perms[3] = 'x';
  }
  if (mode & S_IRGRP) {
    perms[4] = 'r';
  }
  if (mode & S_IWGRP) {
    perms[5] = 'w';
  }
  if (mode & S_IXGRP) {
    perms[6] = 'x';
  }
  if (mode & S_IROTH) {
    perms[7] = 'r';
  }
  if (mode & S_IWOTH) {
    perms[8] = 'w';
  }
  if (mode & S_IXOTH) {
    perms[9] = 'x';
  }

  // 获取特殊权限位，会覆盖对应组的执行位
  if (mode & S_ISUID) {
    perms[3] = (perms[3] == 'x') ? 's' : 'S';
  }
  if (mode & S_ISGID) {
    perms[6] = (perms[6] == 'x') ? 's' : 'S';
  }
  if (mode & S_ISVTX) {
    perms[9] = (perms[9] == 'x') ? 't' : 'T';
  }

  return perms;
}

std::string statbuf_get_date(const struct stat* sbuf) {
  const time_t now = time(nullptr);
  const char* fmt =
      (sbuf->st_mtime > now || now - sbuf->st_mtime > 60 * 60 * 24 * 182)
          ? "%b %e    %Y"
          : "%b %e %H:%M";
  struct tm tmbuf{};
  localtime_r(&sbuf->st_mtime, &tmbuf);
  char buf[64];
  if (strftime(buf, sizeof(buf), fmt, &tmbuf) == 0) {
    return "";
  }
  return buf;
}

int lock_internal(int fd, int lock_type) {
  int ret;
  struct flock the_lock{};
  the_lock.l_type = static_cast<short>(lock_type);
  the_lock.l_whence = SEEK_SET;
  the_lock.l_start = 0;
  the_lock.l_len = 0;  // 将整个文件都加锁
  do {
    ret = fcntl(fd, F_SETLKW, &the_lock);
  } while (ret < 0 && errno == EINTR);

  return ret;
}

int lock_file_read(int fd) { return lock_internal(fd, F_RDLCK); }

int lock_file_write(int fd) { return lock_internal(fd, F_WRLCK); }

int unlock_file(int fd) {
  int ret;
  struct flock the_lock{};
  the_lock.l_type = F_UNLCK;
  the_lock.l_whence = SEEK_SET;
  the_lock.l_start = 0;
  the_lock.l_len = 0;  // 将整个文件都加锁
  ret = fcntl(fd, F_SETLKW, &the_lock);
  return ret;
}

int64_t get_time_sec() {
  return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

int64_t get_time_usec() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// 开启套接字fd接受带外数据的功能
void activate_oobinline(int fd) {
  int oob_inline = 1;
  int ret =
      setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &oob_inline, sizeof(oob_inline));
  if (ret == -1) {
    ERR_EXIT("setsockopt");
  }
}

// 当文件描述符fd上有带外数据的时候,将产生sigurg信号
// 该函数设定当前进程能够接受fd文件描述符所产生的sigurg信号
void activate_sigurg(int fd) {
  int ret = fcntl(fd, F_SETOWN, getpid());
  if (ret == -1) {
    ERR_EXIT("fcntl");
  }
}