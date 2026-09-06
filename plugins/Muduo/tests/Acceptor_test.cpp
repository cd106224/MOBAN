#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/socketOps.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

// Acceptor 是构建在 EventLoop 之上的组件,它的创建、listen、销毁都必须在运行
// EventLoop 的 IO 线程里完成(Channel::remove 会做线程校验)。因此这里通过
// EventLoopThread 专门起一个 IO 线程,用 runInLoop 把对 Acceptor 的操作投递到
// 该线程执行;断言全部放在主测试线程,避免 gtest 断言被跨线程调用。
// 跨线程的数据传递用 promise/future 同步,不引入裸共享变量竞争。

namespace {

using Muduo::Acceptor;
using Muduo::EventLoop;
using Muduo::EventLoopThread;
using Muduo::InetAddress;

constexpr std::chrono::milliseconds kWaitTimeout(3000);

// 在 IO 线程上执行 fn,并阻塞当前线程直到其执行完毕。
void runOnLoopAndWait(EventLoop* loop, const std::function<void()>& fn) {
  std::promise<void> done;
  loop->runInLoop([&fn, &done] {
    fn();
    done.set_value();
  });
  done.get_future().wait_for(kWaitTimeout);
}

// 在 IO 线程上创建 Acceptor(绑定 127.0.0.1:port),可选立即 listen。
// 返回前保证创建(以及 listen)已在 IO 线程完成,失败时返回 nullptr。
Acceptor* startAcceptor(EventLoop* loop, uint16_t port,
                        const Acceptor::NewConnectionCallback& cb,
                        bool listenNow = true) {
  std::promise<Acceptor*> ready;
  loop->runInLoop([&] {
    auto* acceptor = new Acceptor(loop, InetAddress("127.0.0.1", port));
    if (cb) {
      acceptor->setNewConnectionCallback(cb);
    }
    if (listenNow) {
      acceptor->listen();
    }
    ready.set_value(acceptor);
  });
  auto future = ready.get_future();
  if (future.wait_for(kWaitTimeout) != std::future_status::ready) {
    return nullptr;
  }
  return future.get();
}

// RAII:在 IO 线程上删除 Acceptor。保证用例提前 return(ASSERT_*)时不泄漏,
// 且析构始终发生在正确的线程。
struct AcceptorGuard {
  EventLoop* loop;
  Acceptor* ptr;

  AcceptorGuard(EventLoop* l, Acceptor* a) : loop(l), ptr(a) {}
  AcceptorGuard(const AcceptorGuard&) = delete;
  AcceptorGuard& operator=(const AcceptorGuard&) = delete;

  ~AcceptorGuard() {
    if (ptr == nullptr) {
      return;
    }
    std::promise<void> done;
    loop->runInLoop([this, &done] {
      delete ptr;
      ptr = nullptr;
      done.set_value();
    });
    done.get_future().wait_for(kWaitTimeout);
  }
};

// 探测一个当前空闲的 TCP 端口(先绑定端口 0 让内核分配,随后释放)。
uint16_t findFreePort() {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return 0;
  }
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return 0;
  }
  socklen_t len = sizeof(addr);
  if (::getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
    ::close(fd);
    return 0;
  }
  ::close(fd);
  return ntohs(addr.sin_port);
}

// 阻塞式 TCP 客户端 fd 的 RAII 封装。
struct TcpClientFd {
  int fd = -1;

  static TcpClientFd connectTo(uint16_t port) {
    TcpClientFd c;
    c.fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (c.fd < 0) {
      return c;
    }
    const InetAddress server("127.0.0.1", port);
    if (Muduo::sockets::connect(c.fd, server.getSockAddrInet()) < 0) {
      ::close(c.fd);
      c.fd = -1;
    }
    return c;
  }

  TcpClientFd() = default;
  TcpClientFd(TcpClientFd&& other) noexcept : fd(other.fd) { other.fd = -1; }
  TcpClientFd& operator=(TcpClientFd&& other) noexcept {
    if (this != &other) {
      if (fd >= 0) {
        ::close(fd);
      }
      fd = other.fd;
      other.fd = -1;
    }
    return *this;
  }
  TcpClientFd(const TcpClientFd&) = delete;
  TcpClientFd& operator=(const TcpClientFd&) = delete;

  ~TcpClientFd() {
    if (fd >= 0) {
      ::close(fd);
    }
  }
};

// 阻塞读取直到对端关闭。timeoutSec 内读到 EOF 返回 true;超时/出错返回 false。
bool readUntilEof(int fd, std::string* out, int timeoutSec = 3) {
  struct timeval tv{timeoutSec, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  char buf[128];
  for (;;) {
    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n > 0) {
      out->append(buf, static_cast<size_t>(n));
    } else if (n == 0) {
      return true;  // EOF
    } else {
      return false;  // 超时或出错
    }
  }
}

}  // namespace

// 夹具:每个用例独立起一个 IO 线程跑 EventLoop
class AcceptorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = std::make_unique<EventLoopThread>();
    loop_ = thread_->startLoop();
    ASSERT_NE(loop_, nullptr);
  }

  void TearDown() override {
    // EventLoopThread 析构会 quit() 并 join IO 线程
    thread_.reset();
  }

  std::unique_ptr<EventLoopThread> thread_;
  EventLoop* loop_ = nullptr;
};

// ==================== 生命周期 ====================
TEST_F(AcceptorTest, ListeningState) {
  const uint16_t port = findFreePort();
  ASSERT_GT(port, 0);

  // 先创建但不 listen,listening() 应为 false
  AcceptorGuard guard(loop_,
                      startAcceptor(loop_, port, {}, /*listenNow=*/false));
  ASSERT_NE(guard.ptr, nullptr);
  EXPECT_FALSE(guard.ptr->listening());

  runOnLoopAndWait(loop_, [ptr = guard.ptr] { ptr->listen(); });
  EXPECT_TRUE(guard.ptr->listening());

  // listen() 幂等:重复调用不应破坏内部状态
  runOnLoopAndWait(loop_, [ptr = guard.ptr] { ptr->listen(); });
  EXPECT_TRUE(guard.ptr->listening());
}

// ==================== 基本接入:对端信息 + 数据回写 ====================
TEST_F(AcceptorTest, AcceptSingleConnection) {
  const uint16_t port = findFreePort();
  ASSERT_GT(port, 0);

  const std::string greeting = "hello, muduo!\n";
  std::promise<void> accepted;
  InetAddress seenPeer(0);
  int seenFd = -1;

  const Acceptor::NewConnectionCallback cb = [&](int sockfd,
                                                 const InetAddress& peerAddr) {
    // 写回一行数据后关闭,模拟最简单的服务端行为
    (void)::write(sockfd, greeting.data(),
                  static_cast<size_t>(greeting.size()));
    Muduo::sockets::close(sockfd);
    seenFd = sockfd;
    seenPeer = peerAddr;
    accepted.set_value();
  };

  AcceptorGuard guard(loop_, startAcceptor(loop_, port, cb));
  ASSERT_NE(guard.ptr, nullptr);

  TcpClientFd client = TcpClientFd::connectTo(port);
  ASSERT_GE(client.fd, 0);

  // 客户端应完整收到服务器数据,并读到 EOF(FIN)
  std::string data;
  ASSERT_TRUE(readUntilEof(client.fd, &data)) << "客户端应收到数据并读到 EOF";
  EXPECT_EQ(data, greeting);

  // 等待 accept 回调写完共享字段(建立 happens-before)后再读取
  auto future = accepted.get_future();
  ASSERT_EQ(future.wait_for(kWaitTimeout), std::future_status::ready);

  EXPECT_GE(seenFd, 0);                     // 回调拿到了有效 connfd
  EXPECT_EQ(seenPeer.toIp(), "127.0.0.1");  // 对端地址正确

  // 服务器看到的对端端口 == 客户端本地端口
  const struct sockaddr_in local = Muduo::sockets::getLocalAddr(client.fd);
  EXPECT_EQ(ntohs(local.sin_port), ntohs(seenPeer.portNetEndian()));
}

// ==================== 并发多连接 ====================
TEST_F(AcceptorTest, AcceptMultipleConcurrentConnections) {
  constexpr int kConnections = 5;
  const uint16_t port = findFreePort();
  ASSERT_GT(port, 0);

  std::atomic<int> acceptedCount{0};
  std::promise<void> allAccepted;

  const Acceptor::NewConnectionCallback cb = [&](int sockfd,
                                                 const InetAddress&) {
    Muduo::sockets::close(sockfd);
    if (acceptedCount.fetch_add(1) + 1 == kConnections) {
      allAccepted.set_value();
    }
  };

  AcceptorGuard guard(loop_, startAcceptor(loop_, port, cb));
  ASSERT_NE(guard.ptr, nullptr);

  std::vector<TcpClientFd> clients;
  clients.reserve(kConnections);
  for (int i = 0; i < kConnections; ++i) {
    clients.push_back(TcpClientFd::connectTo(port));
    ASSERT_GE(clients.back().fd, 0) << "第 " << i << " 个客户端连接失败";
  }

  auto future = allAccepted.get_future();
  ASSERT_EQ(future.wait_for(kWaitTimeout), std::future_status::ready)
      << "应接收 " << kConnections << " 个连接";
  EXPECT_EQ(acceptedCount.load(), kConnections);

  // 服务端关闭了每个连接,客户端应逐个读到 EOF
  for (auto& client : clients) {
    std::string data;
    EXPECT_TRUE(readUntilEof(client.fd, &data)) << "服务端应关闭每个连接";
  }
}

// ==================== 未注册回调时自动关闭 ====================
TEST_F(AcceptorTest, ClosesWhenNoCallbackRegistered) {
  const uint16_t port = findFreePort();
  ASSERT_GT(port, 0);

  // 不注册回调:Acceptor 收到连接后应主动 close,而不是悬挂
  AcceptorGuard guard(loop_, startAcceptor(loop_, port, {}));
  ASSERT_NE(guard.ptr, nullptr);

  TcpClientFd client = TcpClientFd::connectTo(port);
  ASSERT_GE(client.fd, 0);

  std::string data;
  EXPECT_TRUE(readUntilEof(client.fd, &data, 3))
      << "没有回调时连接应立即被服务器关闭(读到 EOF)";
  EXPECT_TRUE(data.empty());
}

// ==================== 销毁后不再接受连接 ====================
TEST_F(AcceptorTest, RejectsConnectionAfterDestroy) {
  const uint16_t port = findFreePort();
  ASSERT_GT(port, 0);

  auto* acceptor = startAcceptor(
      loop_, port,
      [](int sockfd, const InetAddress&) { Muduo::sockets::close(sockfd); });
  ASSERT_NE(acceptor, nullptr);

  // 存活期间能正常接入
  {
    TcpClientFd client = TcpClientFd::connectTo(port);
    ASSERT_GE(client.fd, 0);
    std::string data;
    EXPECT_TRUE(readUntilEof(client.fd, &data, 3));
  }

  // 在 IO 线程上销毁 Acceptor(拆除 Channel 与监听 socket)
  runOnLoopAndWait(loop_, [acceptor] { delete acceptor; });

  // 监听端口已关闭,新连接应被拒绝
  TcpClientFd refused = TcpClientFd::connectTo(port);
  EXPECT_LT(refused.fd, 0) << "Acceptor 销毁后监听端口应关闭";
}
