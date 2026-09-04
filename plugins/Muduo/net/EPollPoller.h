#pragma once

/**
 * @brief  EPoll封装的Poller
 * @author guogang
 */

#include <sys/epoll.h>
#include <utilities/noncopyable.h>

#include <map>
#include <vector>

#include "base/timestamp.h"

namespace Muduo {
class Channel;
class EventLoop;

class EPollPoller : noncopyable {
 public:
  using ChannelList = std::vector<Channel*>;
  explicit EPollPoller(EventLoop* loop);
  ~EPollPoller();

  // 封装了epoll_wait
  Timestamp poll(int32_t timeoutMs, ChannelList* activeChannels);
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  void assertInLoopThread();

 protected:
  static const int KInitEventListSize = 16;
  void fillActiveChannel(int numEvents, ChannelList* activeChannels);
  void update(int operation, Channel* channel);

 protected:
  using EventList = std::vector<epoll_event>;
  using ChannelMap = std::map<int, Channel*>;

  int epollfd_;
  EventList events_;
  ChannelMap channels_;
  EventLoop* loop_;  // Poller所属EventLoop
};
}  // namespace Muduo