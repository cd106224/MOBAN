#pragma once

/**
 * @brief  负责注册与响应IO事件
 * @author guogang
 */

#include <utilities/noncopyable.h>

#include <functional>
#include <memory>

#include "base/timestamp.h"

namespace Muduo {
class EventLoop;

class Channel : public noncopyable {
 public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(Timestamp)>;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(const Timestamp& receiveTime);
  void setReadCallback(const ReadEventCallback& cb);
  void setWriteCallback(const EventCallback& cb);
  void setCloseCallback(const EventCallback& cb);
  void setErrorCallback(const EventCallback& cb);

  void tie(const std::shared_ptr<void>&);
  [[nodiscard]] int fd() const;
  int events();
  void set_revents(int revt);
  bool isNoneEvent();
  void enableReading();
  void enableWriting();
  void disableWriting();
  void disableAll();
  bool isWriting();
  int index();
  void set_index(int idx);
  void doNotLogHup();
  EventLoop* ownerLoop();
  // 去除通道
  void remove();

 protected:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int KNoneEvent;
  static const int KReadEvent;
  static const int KWriteEvent;

  EventLoop* loop_;  // 所属EventLoop
  const int fd_;     // 文件描述符,但不负责关闭该文件描述符
  int events_;
  int revents_;
  int index_;
  int logHup_;  // for POLLHUP

  /*
  EventLoop 是异步分发事件的：poll() 返回、到真正调用 handleEvent()
  之间有时间差。在这段时间里，连接可能已经被关闭并销毁了（比如业务线程主动断开、或上一次事件里把连接移除了）。如果宿主
  TcpConnection 被销毁，它持有的 Channel 及回调就变成悬垂指针——handleEvent
  再执行回调就是访问已释放内存。
  */
  std::weak_ptr<void> tie_;  // 跟tcp生存期是相关的
  bool tied_;
  bool eventHandling_;
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}  // namespace Muduo