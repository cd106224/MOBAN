#pragma once

#include <arpa/inet.h>

namespace Muduo::sockets {
int createNonblockingOrDie();
void setNonblockAndCloseOnExec(int sockfd);
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);
void toIp(char* buf, size_t size, const struct sockaddr_in& addr);
void toIpPort(char* buf, size_t size, const struct sockaddr_in& addr);
void close(int sockdf);
void bindOrDie(int sockfd, const struct sockaddr_in& addr);
void listenOrDie(int sockfd);
int accept(int sockfd, struct sockaddr_in* addr);
void shutdownWrite(int sockfd);
int connect(int sockfd, const struct sockaddr_in& addr);
int getSocketError(int sockfd);
bool isSelfConnect(int sockfd);
sockaddr_in getLocalAddr(int sockfd);
sockaddr_in getPeerAddr(int sockfd);
}  // namespace Muduo::sockets