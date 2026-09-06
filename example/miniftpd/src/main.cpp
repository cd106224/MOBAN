#include <log/logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <unistd.h>

#include <memory>

#include "ftp_session.h"
#include "thread_pool.h"

int main() {
  if (getuid() != 0) {
    // 绑定 21 等特权端口需要 root;这里监听 8888 所以普通用户也能跑
    LOG_WARN("miniftpd:not running as root");
  }

  // 磁盘 IO 线程池:进程级共享,所有会话的 read/readdir/stat 都提交到这里。
  // IO 线程因此永远不被慢磁盘阻塞(普通文件 fd 无法非阻塞读,
  // 在 IO 线程里读盘等于冻住整个 loop)。线程数 = 磁盘并发度,
  // NVMe 可取 4~8,HDD 单队列深度收益小,2 即可
  ftp::ThreadPool diskPool(4);

  Muduo::EventLoop loop;
  Muduo::InetAddress inetaddress(8888);
  Muduo::TcpServer tcp_server(&loop, inetaddress, "miniftpd");
  // 多 IO 线程:每个客户端会话(控制+数据连接)被固定到其中一条线程,
  // 线程之间互不共享会话状态,无需加锁(见 FtpSession 头注释)
  tcp_server.setThreadNum(2);

  // 注意回调线程:connection/message 回调由【该连接所属 IO 线程】执行,
  // 因此这里的 conn->getLoop() 是 IO 线程的 loop,不是主线程的 loop。
  // 把 IO loop 传给 FtpSession,它的数据 TcpServer 也会建在同一线程上
  tcp_server.setConnectionCallback(
      [&diskPool](const Muduo::TcpConnectionPtr& conn) {
        if (conn->connected()) {
          // 一条控制连接 = 一个 FtpSession,挂在 conn 的 context 上,
          // 连接销毁时 context 复位,FtpSession 连带析构
          auto session =
              std::make_shared<ftp::FtpSession>(conn->getLoop(), &diskPool);
          conn->setContext(session);
          session->start(conn);
        } else {
          conn->setContext(std::any{});
        }
      });

  tcp_server.setMessageCallback([](const Muduo::TcpConnectionPtr& conn,
                                   Muduo::Buffer* buffer, Timestamp time) {
    auto* ctx = conn->getMutableContext();
    if (ctx->has_value()) {
      auto session = std::any_cast<std::shared_ptr<ftp::FtpSession>>(*ctx);
      session->onMessage(conn, buffer, time);
    }
  });

  tcp_server.start();
  loop.loop();
  return 0;
}
