#include "ftp_session.h"

#include <dirent.h>
#include <fcntl.h>
#include <muduo/net/socketOps.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>

#include "thread_pool.h"

namespace ftp {
namespace {

// FTP 行协议以 CRLF 结尾
constexpr const char* kCRLF = "\r\n";

// 服务器对外展示的根目录(相对该目录,客户端看不到外层文件系统)
constexpr const char* kFtpRoot = ".";

// 每次从磁盘读多少字节交给发送缓冲
// 过小:系统调用频繁;过大:单块占用过多内存与调度延迟。
// 64KB 在吞吐与内存之间是常见折中(与内核 socket 缓冲默认值同量级)
constexpr size_t kReadChunkSize = 64 * 1024;

// LIST 每批处理的目录项数:IO 线程 readdir 取这么多个名字,
// 交给池线程批量 stat,回来 send 后由 writeComplete 驱动下一批
constexpr size_t kListBatchEntries = 64;

std::string unixListLine(const struct stat& st, const std::string& name) {
  char mode[] = "----------";
  if (S_ISDIR(st.st_mode)) mode[0] = 'd';
  if (st.st_mode & S_IRUSR) mode[1] = 'r';
  if (st.st_mode & S_IWUSR) mode[2] = 'w';
  if (st.st_mode & S_IXUSR) mode[3] = 'x';
  if (st.st_mode & S_IRGRP) mode[4] = 'r';
  if (st.st_mode & S_IWGRP) mode[5] = 'w';
  if (st.st_mode & S_IXGRP) mode[6] = 'x';
  if (st.st_mode & S_IROTH) mode[7] = 'r';
  if (st.st_mode & S_IWOTH) mode[8] = 'w';
  if (st.st_mode & S_IXOTH) mode[9] = 'x';

  char timeBuf[64];
  struct tm tmv{};
  localtime_r(&st.st_mtime, &tmv);
  strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M", &tmv);

  char line[512];
  snprintf(line, sizeof(line), "%s %3lu %-8u %-8u %8lu %s %s", mode,
           static_cast<unsigned long>(st.st_nlink), st.st_uid, st.st_gid,
           static_cast<unsigned long>(st.st_size), timeBuf, name.c_str());
  return line;
}

}  // namespace

FtpSession::FtpSession(Muduo::EventLoop* loop, ThreadPool* diskPool)
    : loop_(loop), diskPool_(diskPool) {}

FtpSession::~FtpSession() {
  if (xferFd_ >= 0) {
    ::close(xferFd_);
  }
  if (xferDir_ != nullptr) {
    ::closedir(xferDir_);
  }
  closeDataServer();
}

void FtpSession::start(const Muduo::TcpConnectionPtr& ctrlConn) {
  ctrlConn_ = ctrlConn;
  // FTP 规定服务器先说话
  reply(ctrlConn_, "220 miniftpd ready");
}

void FtpSession::reply(const Muduo::TcpConnectionPtr& conn,
                       const std::string& text) {
  std::string msg = text + kCRLF;
  conn->send(msg);
  LOG_INFO("ftp reply -> {} : {}", conn->name(), text);
}

void FtpSession::onMessage(const Muduo::TcpConnectionPtr& conn,
                           Muduo::Buffer* buffer, Timestamp /*time*/) {
  // 按行拆分 FTP 命令,可能一次到达多行(粘包),也可能半行
  while (buffer->readableBytes() > 0) {
    const char* crlf = buffer->findCRLF();
    if (crlf == nullptr) {
      // 无 CRLF 且缓冲已超长:可能是恶意输入,断开而非无限缓冲
      if (buffer->readableBytes() > 4096) {
        LOG_WARN("control line too long, closing {}", conn->name());
        conn->forceClose();
        return;
      }
      break;  // 剩余是半条命令,等下一次可读事件
    }
    std::string line(buffer->peek(), crlf - buffer->peek());
    buffer->retrieveUntil(crlf + 2);
    handleCommand(conn, line);
  }
}

void FtpSession::handleCommand(const Muduo::TcpConnectionPtr& conn,
                               const std::string& line) {
  LOG_INFO("ftp cmd <- {} : {}", conn->name(), line);
  // 命令格式: "CMD [arg]",大小写不敏感
  std::string cmd = line;
  std::string arg;
  size_t sp = line.find(' ');
  if (sp != std::string::npos) {
    cmd = line.substr(0, sp);
    arg = line.substr(sp + 1);
  }
  std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  if (cmd == "USER") return cmdUser(conn, arg);
  if (cmd == "PASS") return cmdPass(conn, arg);
  if (cmd == "SYST") return cmdSyst(conn);
  if (cmd == "PWD" || cmd == "XPWD") return cmdPwd(conn);
  if (cmd == "TYPE") return cmdType(conn);
  if (cmd == "PASV") return cmdPasv(conn);
  if (cmd == "LIST") return cmdList(conn, /*longNames=*/true);
  if (cmd == "NLST") return cmdList(conn, /*longNames=*/false);
  if (cmd == "RETR") return cmdRetr(conn, arg);
  if (cmd == "QUIT") return cmdQuit(conn);

  reply(conn, "502 command not implemented");
}

bool FtpSession::requireLogin(const Muduo::TcpConnectionPtr& conn,
                              bool loggedIn) {
  if (!loggedIn) {
    reply(conn, "530 please login with USER and PASS");
  }
  return loggedIn;
}

// ---------------------------------------------------------------------------
// 命令实现
// ---------------------------------------------------------------------------

void FtpSession::cmdUser(const Muduo::TcpConnectionPtr& conn,
                         const std::string& /*arg*/) {
  state_ = State::kNeedPass;
  reply(conn, "331 password required");
}

void FtpSession::cmdPass(const Muduo::TcpConnectionPtr& conn,
                         const std::string& /*arg*/) {
  if (state_ != State::kNeedPass) {
    reply(conn, "503 login with USER first");
    return;
  }
  state_ = State::kLoggedIn;
  reply(conn, "230 login successful");
}

void FtpSession::cmdSyst(const Muduo::TcpConnectionPtr& conn) {
  reply(conn, "215 UNIX Type: L8");
}

void FtpSession::cmdPwd(const Muduo::TcpConnectionPtr& conn) {
  if (!requireLogin(conn, state_ == State::kLoggedIn)) return;
  reply(conn, "257 \"" + cwd_ + "\" is the current directory");
}

void FtpSession::cmdType(const Muduo::TcpConnectionPtr& conn) {
  if (!requireLogin(conn, state_ == State::kLoggedIn)) return;
  // 统一按二进制处理, I/A 都接受
  reply(conn, "200 type set to I");
}

// ---------------------------------------------------------------------------
// PASV:客户端要求服务器开一个数据监听端口
// ---------------------------------------------------------------------------

std::string FtpSession::pasvReply(uint32_t ipNetEndian,
                                  uint16_t portNetEndian) {
  // 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
  const auto* ip = reinterpret_cast<const unsigned char*>(&ipNetEndian);
  const auto* pt = reinterpret_cast<const unsigned char*>(&portNetEndian);
  char buf[64];
  snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)",
           ip[0], ip[1], ip[2], ip[3], pt[0], pt[1]);
  return buf;
}

void FtpSession::cmdPasv(const Muduo::TcpConnectionPtr& conn) {
  if (!requireLogin(conn, state_ == State::kLoggedIn)) return;
  if (dataServer_) {
    closeDataServer();  // 上一条数据连接还没结束又来 PASV,直接复用端口会失败
  }

  // 关键点1:数据服务器建在【会话所在线程的 loop】(ctrlConn_->getLoop()),
  // 而不是某个全局 loop。这样控制连接与数据连接的所有回调都在同一个
  // IO 线程串行执行,会话状态无需加锁——这是多线程 Reactor 下保持
  // "一个会话一条线程"的正确姿势,setThreadNum(N>0) 也安全。
  //
  // 关键点2:监听 0 端口由内核分配,start() 后 listenAddr() 回读实际端口,
  // 端口全程未归还内核,不存在被其他进程抢走的竞争窗口。
  dataServer_ = std::make_unique<Muduo::TcpServer>(
      ctrlConn_->getLoop(), Muduo::InetAddress(0), "ftpData");
  dataServer_->setConnectionCallback(
      [self = shared_from_this()](const Muduo::TcpConnectionPtr& c) {
        self->handleDataConnected(c);
      });
  dataServer_->setMessageCallback(
      [self = shared_from_this()](const Muduo::TcpConnectionPtr& c,
                                  Muduo::Buffer* b, Timestamp t) {
        self->handleDataMessage(c, b, t);
      });
  dataServer_->start();

  // 打开端口后必须限时:客户端若不发起传输,端口会被永久占用。
  // 保存 TimerId,传输开始/收尾时显式 cancel,定时器对象不留悬挂
  auto self = shared_from_this();
  pasvTimerId_ = loop_->runAfter(kPasvTimeoutSec, [self] {
    // 仅当端口还空着(没传输任务也没数据连接)才回收
    if (self->dataServer_ && self->xfer_ == Xfer::kNone && !self->dataConn_) {
      LOG_INFO("pasv timeout, closing data listener");
      self->reply(self->ctrlConn_, "425 passive mode timed out");
      self->closeDataServer();
    }
  });

  const uint16_t port = ntohs(dataServer_->listenAddr().portNetEndian());
  const uint32_t ipNet = conn->localAddress().ipNetEndian();
  reply(conn, pasvReply(ipNet, dataServer_->listenAddr().portNetEndian()));
  LOG_INFO("ftp pasv listening on port {}", port);
}

void FtpSession::handleDataConnected(const Muduo::TcpConnectionPtr& conn) {
  if (conn->connected()) {
    if (dataConn_) {
      // 已有一条数据连接:拒绝第二个(端口扫描/重复连接),立即关闭
      LOG_WARN("unexpected second data connection from {}",
               conn->peerAddress().toIpPort());
      conn->forceClose();
      return;
    }
    dataConn_ = conn;
    // 发送缓冲清空(数据已进内核)时触发:请求下一块——传输的"泵"
    conn->setWriteCompleteCallback(
        [self = shared_from_this()](const Muduo::TcpConnectionPtr&) {
          if (self->xfer_ != Xfer::kNone) {
            self->requestNextChunk();
          }
        });
    LOG_INFO("ftp data connection up: {}", conn->name());
    tryStartTransfer();
  } else {
    LOG_INFO("ftp data connection down: {}", conn->name());
    if (xfer_ != Xfer::kNone && !xferDone_) {
      // 服务器还没发完就对端断开:传输被客户端中断
      abortTransfer();
      reply(ctrlConn_, "426 connection closed; transfer aborted");
    } else {
      // 正常收尾:服务器 shutdown 后对端关闭,回 226
      sendDataEndReply();
    }
    loop_->queueInLoop(
        [self = shared_from_this()] { self->closeDataServer(); });
  }
}

void FtpSession::tryStartTransfer() {
  if (!dataConn_ || !dataConn_->connected() || xfer_ == Xfer::kNone) {
    return;  // 还有一侧未就绪
  }

  if (xfer_ == Xfer::kRetr) {
    xferFd_ = ::open(xferFile_.c_str(), O_RDONLY | O_CLOEXEC);
    if (xferFd_ < 0) {
      reply(ctrlConn_, "550 failed to open file");
      dataConn_->forceClose();
      loop_->queueInLoop(
          [self = shared_from_this()] { self->closeDataServer(); });
      return;
    }
  } else {  // kList:目录句柄由 IO 线程持有,跨批次复用(见 requestNextChunk)
    xferDir_ = ::opendir((std::string(kFtpRoot) + cwd_).c_str());
    if (xferDir_ == nullptr) {
      reply(ctrlConn_, "550 failed to list directory");
      dataConn_->forceClose();
      loop_->queueInLoop(
          [self = shared_from_this()] { self->closeDataServer(); });
      return;
    }
  }
  requestNextChunk();
}

// 传输的发起方(仅 IO 线程调用,由消息回调与 writeComplete 驱动):
// 捕获当前传输代数,把磁盘工作打包进线程池,立即返回——IO 线程绝不等待磁盘。
// RETR 用 pread(显式偏移,不依赖 fd 当前位置,池线程并发安全);
// LIST 的分工见下
void FtpSession::requestNextChunk() {
  if (xfer_ == Xfer::kNone || !dataConn_ || !dataConn_->connected()) {
    return;
  }
  const uint64_t gen = xferGen_;

  if (xfer_ == Xfer::kRetr) {
    off_t offset = nextReadOffset_;
    nextReadOffset_ += static_cast<off_t>(kReadChunkSize);
    int fd = xferFd_;
    if (!diskPool_->trySubmit([self = shared_from_this(), gen, fd, offset] {
          std::string chunk(kReadChunkSize, '\0');
          ssize_t n = ::pread(fd, chunk.data(), kReadChunkSize, offset);
          int err = 0;
          if (n < 0) {
            err = errno;
            n = 0;
          }
          chunk.resize(static_cast<size_t>(n));
          // 回 IO 线程发送(磁盘耗时全部发生在池线程)
          self->loop_->queueInLoop([self, gen, chunk = std::move(chunk), err] {
            self->sendChunkReady(gen, std::move(chunk),
                                 err == 0 && chunk.empty(), err != 0);
          });
        })) {
      transferRejected("disk pool overloaded");
      return;
    }
    return;
  }

  // LIST 的异步化困境与解法:
  //   DIR* 有内部读取位置,不能跨线程并发使用(readdir 竞争是 UB);
  //   每批重新 opendir 又会从头遍历,造成无限循环。
  //   关键观察:readdir 走内核目录页缓存,通常不碰磁盘(微秒级);
  //   真正可能阻塞的是每个条目的 stat(冷 inode 时毫秒级一次)。
  //   所以分工是:readdir 留在 IO 线程(拿一批名字,安全:DIR* 只在
  //   IO 线程使用),批量 stat 交给池线程,结果回 IO 线程拼行 send。
  //   背压得以保留:writeComplete 驱动下一批,网络慢则不再取名字
  std::string dirPath = std::string(kFtpRoot) + cwd_;
  std::vector<std::string> names;
  names.reserve(kListBatchEntries);
  bool dirError = false;
  if (longNameList_) {
    struct dirent* ent = nullptr;
    while (names.size() < kListBatchEntries &&
           (ent = ::readdir(xferDir_)) != nullptr) {
      names.emplace_back(ent->d_name);
    }
  } else {
    // NLST 只列普通文件(不含 . .. 和目录),同样分批
    struct dirent* ent = nullptr;
    while (names.size() < kListBatchEntries &&
           (ent = ::readdir(xferDir_)) != nullptr) {
      names.emplace_back(ent->d_name);
    }
  }
  if (names.empty()) {
    // 目录读尽:直接收尾,不需要池线程
    sendChunkReady(gen, std::string(), true, false);
    return;
  }

  if (!diskPool_->trySubmit([self = shared_from_this(), gen, dirPath,
                             names = std::move(names),
                             longList = longNameList_] {
        std::string chunk;
        if (!longList) {
          // NLST:纯文件名,无需 stat
          for (const auto& name : names) {
            chunk += name + kCRLF;
          }
        } else {
          for (const auto& name : names) {
            struct stat st{};
            if (::stat((dirPath + "/" + name).c_str(), &st) == 0) {
              chunk += unixListLine(st, name) + kCRLF;
            }
          }
        }
        self->loop_->queueInLoop([self, gen, chunk = std::move(chunk)] {
          self->sendChunkReady(gen, std::move(chunk), false, false);
        });
      })) {
    transferRejected("disk pool overloaded");
    return;
  }
}

// 线程池拒绝任务(队列满,磁盘严重过载):终止本次传输。
// 必须快速失败而不是阻塞或无限排队——IO 线程的存活优先于单个传输
void FtpSession::transferRejected(const char* why) {
  LOG_WARN("transfer aborted: {}", why);
  reply(ctrlConn_, "451 local error in processing");
  abortTransfer();
  if (dataConn_) {
    dataConn_->forceClose();
  }
  loop_->queueInLoop([self = shared_from_this()] { self->closeDataServer(); });
}

void FtpSession::sendChunkReady(uint64_t gen, std::string chunk, bool atEof,
                                bool ioError) {
  // 代数不匹配:这个结果属于已被中断/完成的旧传输,直接丢弃。
  // 这是异步传输模型的核心防护:abort 后在途任务回来不能污染新状态
  if (gen != xferGen_ || xfer_ == Xfer::kNone) {
    return;
  }
  if (ioError) {
    if (xfer_ == Xfer::kRetr) {
      reply(ctrlConn_, "550 read error during transfer");
    } else {
      reply(ctrlConn_, "550 failed to list directory");
    }
    abortTransfer();
    if (dataConn_) {
      dataConn_->forceClose();
    }
    loop_->queueInLoop(
        [self = shared_from_this()] { self->closeDataServer(); });
    return;
  }

  if (chunk.empty() && atEof) {
    // 到尾:RETR 是 pread 返回 0;LIST 是 readdir 取不到新名字。
    // gen 校验保证这里不会是旧传输的误报
    xferDone_ = true;
    dataConn_->shutdown();  // 发送缓冲清空后自动关写,对端读到 EOF
    return;
  }

  if (!dataConn_ || !dataConn_->connected()) {
    return;  // 回调间隙连接断了,handleDataConnected 的 426 分支会收尾
  }
  dataConn_->send(chunk);
  if (atEof) {
    xferDone_ = true;
    dataConn_->shutdown();  // 缓冲清空后自动关写,对端读到 EOF
  }
}

void FtpSession::handleDataMessage(const Muduo::TcpConnectionPtr& /*conn*/,
                                   Muduo::Buffer* buffer, Timestamp) {
  // 本阶段只实现下载/LIST,数据连接收到的内容(上传)直接丢弃
  buffer->retrieveAll();
}

void FtpSession::abortTransfer() {
  xfer_ = Xfer::kNone;
  xferDone_ = false;
  xferFile_.clear();
  nextReadOffset_ = 0;
  // 代数 +1:在途的线程池任务回来时 gen 不匹配,结果被丢弃。
  // 注意:已提交的 pread/stat 仍会执行(close 后 fd 数值可能被复用,
  // 但读到的错误数据同样会被 gen 拦截,不会发送),只是浪费一次 IO
  ++xferGen_;
  if (xferFd_ >= 0) {
    ::close(xferFd_);
    xferFd_ = -1;
  }
  if (xferDir_ != nullptr) {
    ::closedir(xferDir_);
    xferDir_ = nullptr;
  }
  if (dataConn_) {
    dataConn_->setWriteCompleteCallback({});  // 清除回调,防止误触发
  }
}

void FtpSession::sendDataEndReply() {
  if (xfer_ == Xfer::kList) {
    reply(ctrlConn_, "226 list transfer complete");
  } else if (xfer_ == Xfer::kRetr) {
    reply(ctrlConn_, "226 transfer complete");
  }
  loop_->cancel(pasvTimerId_);  // 传输已结束,PASV 空闲定时器失去意义
  abortTransfer();
}

void FtpSession::closeDataServer() {
  dataConn_.reset();
  if (dataServer_ && loop_) {
    // TcpServer 析构要求在 loop 线程内执行;会话所有回调都在 loop_ 线程,
    // 这里同步释放所有权并把析构排入本线程队列即可
    Muduo::TcpServer* raw = dataServer_.release();
    loop_->queueInLoop([raw] { delete raw; });
  }
}

void FtpSession::cmdList(const Muduo::TcpConnectionPtr& conn, bool longNames) {
  if (!requireLogin(conn, state_ == State::kLoggedIn)) return;
  if (!dataServer_) {
    reply(conn, "425 use PASV first");
    return;
  }
  ++xferGen_;  // 新传输,作废一切在途异步任务
  xfer_ = Xfer::kList;
  longNameList_ = longNames;    // LIST: ls -l 长格式;NLST: 纯文件名
  loop_->cancel(pasvTimerId_);  // 传输已开始,PASV 空闲定时器失去意义
  reply(conn, "150 here comes the directory listing");
  tryStartTransfer();  // 客户端可能已连上数据端口
}

void FtpSession::cmdRetr(const Muduo::TcpConnectionPtr& conn,
                         const std::string& arg) {
  if (!requireLogin(conn, state_ == State::kLoggedIn)) return;
  if (!dataServer_) {
    reply(conn, "425 use PASV first");
    return;
  }
  if (arg.empty()) {
    reply(conn, "501 no file name given");
    return;
  }
  // 只允许相对 kFtpRoot 的路径,拒绝 .. 防止目录穿越
  if (arg.find("..") != std::string::npos || arg.front() == '/') {
    reply(conn, "550 invalid path");
    return;
  }
  std::string fullPath = std::string(kFtpRoot) + "/" + arg;
  struct stat st{};
  if (::stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
    reply(conn, "550 no such file");
    return;
  }
  ++xferGen_;  // 新传输,作废一切在途异步任务
  xfer_ = Xfer::kRetr;
  xferFile_ = fullPath;
  nextReadOffset_ = 0;
  xferDone_ = false;
  loop_->cancel(pasvTimerId_);  // 传输已开始,PASV 空闲定时器失去意义
  reply(conn, "150 opening BINARY mode data connection");
  tryStartTransfer();  // 客户端可能已连上数据端口
}

void FtpSession::cmdQuit(const Muduo::TcpConnectionPtr& conn) {
  reply(conn, "221 bye");
  conn->shutdown();
}

}  // namespace ftp
