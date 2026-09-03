#include "TcpClient.h"

#include "Connector.h"
#include "socketOps.h"

namespace Muduo {
namespace {
void _removeConnection(EventLoop* loop, const TcpConnectionPtr& conn) {
  loop->runInLoop(std::bind(&TcpConnection::connnectDestroyed, conn));
}
}  // namespace

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr,
                     std::string name)
    : loop_(loop),
      connector_(new Connector(loop, serverAddr)),
      name_(std::move(name)),
      ConnectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      retry_(false),
      connect_(false),
      nextConnId_(1) {
  connector_->setNewConnectionCallback(
      std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
}

TcpClient::~TcpClient() {
  TcpConnectionPtr conn;
  {
    std::lock_guard lock(mutex_);
    conn = connection_;
  }
  if (conn) {
    // FIXME: not 100% safe, if we are in different thread
    CloseCallback cb =
        std::bind(_removeConnection, loop_, std::placeholders::_1);
    loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
  } else {
    // 这种情况,说明connector处于未连接状态,将connector_停止
    connector_->stop();
  }
}

void TcpClient::connect() {
  connect_ = true;
  connector_->start();
}

// 用于连接已经建立的情况下,关闭连接
void TcpClient::disconnect()  // 主动断开
{
  connect_ = false;
  {
    std::lock_guard lock(mutex_);
    if (connection_) {
      connection_->shutdown();
    }
  }
}

void TcpClient::stop() {
  connect_ = false;
  connector_->stop();
}

TcpConnectionPtr TcpClient::connection() const {
  std::lock_guard lk(mutex_);
  return connection_;
}

bool TcpClient::retry() const { return retry_; }

void TcpClient::enableRetry() { retry_ = true; }

void TcpClient::setConnectionCallback(const ConnectionCallback& cb) {
  ConnectionCallback_ = cb;
}

void TcpClient::setMessageCallback(const MessageCallback& cb) {
  messageCallback_ = cb;
}

void TcpClient::setWriteCompleteCallback(const WriteCompleteCallback& cb) {
  writeCompleteCallback_ = cb;
}

void TcpClient::newConnection(int sockfd) {
  loop_->assertInLoopThread();
  InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  char buf[32];
  snprintf(buf, sizeof(buf), "%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
  ++nextConnId_;
  std::string m_connName = name_ + buf;

  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  TcpConnectionPtr conn(
      new TcpConnection(loop_, m_connName, sockfd, localAddr, peerAddr));
  conn->setConnectionCallback(ConnectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));
  {
    std::lock_guard lock(mutex_);
    connection_ = conn;
  }
  conn->connectEstablished();  // 这里回调connectionCallback_
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)  // 被动断开
{
  loop_->assertInLoopThread();
  assert(loop_ == conn->getLoop());
  {
    std::lock_guard lock(mutex_);
    assert(connection_ == conn);
    connection_.reset();
  }
  loop_->runInLoop(std::bind(&TcpConnection::connnectDestroyed, conn));
  if (retry_ && connect_) {
    // 这里的重连是指建立成功后被断开的重连
    connector_->restart();
  }
}

}  // namespace Muduo