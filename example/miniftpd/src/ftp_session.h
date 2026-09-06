#pragma once

/**
 * @brief   FTP 会话:一条控制连接对应一个 FtpSession
 *
 * 线程模型(生产式,无锁):
 *   整个会话(控制连接 + 数据连接)固定在控制连接所属的那个 IO 线程里。
 *   main.cpp 的 setThreadNum(N) 决定有 N 个 IO 线程,不同客户端会话
 *   分散到不同线程,但同一会话的所有回调永远串行在同一线程执行,
 *   因此会话内所有成员状态天然无竞争,不需要任何锁。
 *
 *   阻塞隔离:磁盘 IO(read/readdir/stat)不在 IO 线程执行,而是提交给
 *   全局线程池,完成后通过 EventLoop::queueInLoop 交回会话线程继续 send。
 *   IO 线程永远只做内存拷贝级别的工作,不会被慢磁盘卡住。
 *
 *   生命周期:异步任务持有 shared_from_this,回调前会话不会被销毁;
 *   任务内部通过原子代数 xferGen_ 识别"已作废的传输",作废结果直接丢弃。
 *
 * @author  guogang
 */

#include <dirent.h>
#include <log/logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include <cstdint>
#include <memory>
#include <string>

namespace ftp {

class ThreadPool;

class FtpSession : public std::enable_shared_from_this<FtpSession> {
 public:
  // loop 必须传控制连接所属的 IO loop(main.cpp 里用 conn->getLoop()),
  // 会话的所有回调都将在该线程串行执行
  FtpSession(Muduo::EventLoop* loop, ThreadPool* diskPool);
  ~FtpSession();

  void start(const Muduo::TcpConnectionPtr& ctrlConn);

  // 控制连接消息入口(公有:由 main.cpp 的回调调用)
  void onMessage(const Muduo::TcpConnectionPtr& conn, Muduo::Buffer* buffer,
                 Timestamp time);

 private:
  // ---- 控制连接侧 ----
  void handleCommand(const Muduo::TcpConnectionPtr& conn,
                     const std::string& line);
  void reply(const Muduo::TcpConnectionPtr& conn, const std::string& text);

  // ---- PASV 数据连接侧 ----
  void handleDataConnected(const Muduo::TcpConnectionPtr& conn);
  void handleDataMessage(const Muduo::TcpConnectionPtr& conn,
                         Muduo::Buffer* buffer, Timestamp time);
  void closeDataServer();
  // 数据连接与传输任务都就绪时开始传输(客户端连数据端口与发LIST/RETR的
  // 顺序不固定,两侧都调用本函数,谁后到谁触发)
  void tryStartTransfer();
  // 把下一块的磁盘工作提交给线程池;完成后 IO 线程收到 sendChunkReady。
  // RETR:池线程 pread;LIST:IO线程 readdir 取一批名字,池线程批量 stat
  void requestNextChunk();
  // 磁盘工作完成,回到 IO 线程:发送数据或收尾(gen 不匹配则丢弃)
  void sendChunkReady(uint64_t gen, std::string chunk, bool atEof,
                      bool ioError = false);
  // 传输异常终止(客户端中断/打开失败/线程池拒绝):清理资源并销毁数据服务器
  void abortTransfer();
  // submit 被线程池拒绝(磁盘过载)时的统一收尾:应答 4xx 并清理
  void transferRejected(const char* why);

  // ---- 命令实现 ----
  void cmdUser(const Muduo::TcpConnectionPtr& conn, const std::string& arg);
  void cmdPass(const Muduo::TcpConnectionPtr& conn, const std::string& arg);
  void cmdSyst(const Muduo::TcpConnectionPtr& conn);
  void cmdPwd(const Muduo::TcpConnectionPtr& conn);
  void cmdType(const Muduo::TcpConnectionPtr& conn);
  void cmdPasv(const Muduo::TcpConnectionPtr& conn);
  void cmdList(const Muduo::TcpConnectionPtr& conn, bool longNames);
  void cmdRetr(const Muduo::TcpConnectionPtr& conn, const std::string& arg);
  void cmdQuit(const Muduo::TcpConnectionPtr& conn);

  void sendDataEndReply();

  // ---- 辅助 ----
  bool requireLogin(const Muduo::TcpConnectionPtr& conn, bool loggedIn);
  static std::string pasvReply(uint32_t ipNetEndian, uint16_t portNetEndian);

 private:
  enum class State { kNeedUser, kNeedPass, kLoggedIn };

  // 会话专属 IO 线程的 loop;所有回调都在它上面执行
  Muduo::EventLoop* loop_;
  // 全局磁盘线程池(进程级共享,见 main.cpp);其线程数 = 磁盘并发度
  ThreadPool* diskPool_;
  Muduo::TcpConnectionPtr ctrlConn_;

  State state_ = State::kNeedUser;
  std::string cwd_ = "/";

  // PASV 数据服务器:一次传输一个,传完销毁
  std::unique_ptr<Muduo::TcpServer> dataServer_;
  Muduo::TcpConnectionPtr dataConn_;
  enum class Xfer { kNone, kList, kRetr };
  Xfer xfer_ = Xfer::kNone;
  bool longNameList_ = true;  // LIST: ls -l 长格式;NLST: 仅文件名
  std::string xferFile_;
  int xferFd_ = -1;           // RETR:传输中的文件 fd(仅 IO 线程与池线程访问)
  off_t nextReadOffset_ = 0;  // 下次 pread 的偏移(仅 IO 线程推进)
  bool xferDone_ = false;     // 数据写完(shutdown 已调用),等对端关闭收尾
  // 传输代数:每次开始新传输 +1。异步任务捕获发起时的 gen,
  // 回调回来时若与当前值不符,说明该传输已被中断,结果作废
  uint64_t xferGen_ = 0;
  // LIST 的目录句柄:仅 IO 线程访问(readdir 有内部状态,不可跨线程),
  // 池线程只对 IO 线程递来的名字批量做 stat
  DIR* xferDir_ = nullptr;
  // PASV 空闲定时器:传输开始后取消,防止端口被过早回收
  Muduo::TimerId pasvTimerId_;

  // 数据连接建立后允许的等待窗口:客户端必须在此时间内发 LIST/RETR,
  // 否则回收数据端口,防止连接挂起占用资源
  static constexpr int kPasvTimeoutSec = 30;
};

}  // namespace ftp
