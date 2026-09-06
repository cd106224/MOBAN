#include "EPollPoller.h"

#include <sys/epoll.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include "Channel.h"
#include "EventLoop.h"
#include "base/timestamp.h"
#include "log/logging.h"

namespace {
const int KNew = -1;
const int KAdded = 1;
const int KDeleted = 2;
}  // namespace

namespace Muduo {
EPollPoller::EPollPoller(EventLoop* loop)
    : epollfd_(::epoll_create(EPOLL_CLOEXEC)),
      events_(KInitEventListSize),
      loop_(loop) {
  if (epollfd_ < 0) {
    LOG_FATAL("EPollPoller::EPollPoller: epoll_create failed, errno = {}",
              errno);
    exit(EXIT_FAILURE);
  }
}

EPollPoller::~EPollPoller() { close(epollfd_); }

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
  int numEvents = epoll_wait(epollfd_, &*events_.begin(),
                             static_cast<int>(events_.size()), timeoutMs);
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    LOG_INFO("{} events happened", numEvents);
    fillActiveChannel(numEvents, activeChannels);
    if (static_cast<uint64_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() << 1);
    }
  } else if (numEvents == 0) {
    LOG_INFO("nothing happened");
  } else {
    LOG_INFO("EPollPoller::poll()");
  }
  return now;
}

// 分发处理
void EPollPoller::fillActiveChannel(int numEvents,
                                    ChannelList* activeChannels) {
  assert(static_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i) {
    auto channel = static_cast<Channel*>(events_[i].data.ptr);
    channel->set_revents(static_cast<int>(events_[i].events));
    activeChannels->emplace_back(channel);
  }
}

void EPollPoller::updateChannel(Channel* channel) {
  loop_->assertInLoopThread();
  LOG_INFO("fd = {},events= {}", channel->fd(), channel->events());
  const int index = channel->index();
  if (index == KNew || index == KDeleted) {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == KNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else {
      // index==KDeleted
      assert(channels_.find(fd) != channels_.end());
    }
    channel->set_index(KAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(KDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EPollPoller::update(int operation, Channel* channel) {
  epoll_event event{};
  event.events = channel->events();
  event.data.ptr = channel;
  int fd = channel->fd();
  if (epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("epoll_ctl op=EPOLL_CTL_DEL fd={}", fd);
    } else {
      LOG_ERROR("epoll_ctl op ={} fd={}", operation, fd);
      exit(EXIT_FAILURE);
    }
  }
}

void EPollPoller::removeChannel(Channel* channel) {
  loop_->assertInLoopThread();
  int fd = channel->fd();
  LOG_INFO("fd={}", fd);
  int index = channel->index();
  channels_.erase(fd);
  if (index == KAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(KNew);
}

void EPollPoller::assertInLoopThread() { loop_->assertInLoopThread(); }

}  // namespace Muduo