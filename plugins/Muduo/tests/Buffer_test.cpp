#include <fcntl.h>
#include <gtest/gtest.h>
#include <net/buffer.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

// Buffer 是纯内存组件,不需要 IO 线程;仅 readFd 用例涉及真实 fd,
// 通过 pipe 构造,读端设置 O_NONBLOCK 以便验证 EAGAIN 分支。

namespace {

using Muduo::Buffer;

constexpr size_t kCheapPrepend = Buffer::kCheapprepend;
constexpr size_t kInitialSize = Buffer::kInitialSize;

// 以非阻塞模式创建 pipe,返回 {读端, 写端};失败时两者均为 -1。
struct PipeFd {
  int readFd = -1;
  int writeFd = -1;

  static PipeFd createNonblocking() {
    int fds[2];
    if (::pipe(fds) < 0) {
      return {};
    }
    for (int fd : fds) {
      const int flags = ::fcntl(fd, F_GETFL, 0);
      ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return {fds[0], fds[1]};
  }

  PipeFd(const PipeFd&) = delete;
  PipeFd& operator=(const PipeFd&) = delete;

  ~PipeFd() {
    for (int* fd : {&readFd, &writeFd}) {
      if (*fd >= 0) {
        ::close(*fd);
        *fd = -1;
      }
    }
  }
};

}  // namespace

// ==================== 初始状态 ====================
TEST(BufferTest, InitialState) {
  Buffer buf;
  EXPECT_EQ(buf.readableBytes(), 0u);
  EXPECT_EQ(buf.writeableBytes(), kInitialSize);
  EXPECT_EQ(buf.prependableBytes(), kCheapPrepend);

  // 内部布局: [kCheapPrepend 预留][kInitialSize 可写]
  EXPECT_EQ(buf.beginWrite(), buf.peek());
}

// ==================== append / retrieve 基本读写 ====================
TEST(BufferTest, AppendAndRetrieve) {
  Buffer buf;
  buf.append(std::string("hello"));
  EXPECT_EQ(buf.readableBytes(), 5u);
  EXPECT_EQ(std::string(buf.peek(), 5), "hello");

  buf.retrieve(2);
  EXPECT_EQ(buf.readableBytes(), 3u);
  EXPECT_EQ(std::string(buf.peek(), 3), "llo");

  buf.retrieveAll();
  EXPECT_EQ(buf.readableBytes(), 0u);
  EXPECT_EQ(buf.prependableBytes(), kCheapPrepend);
}

TEST(BufferTest, RetrieveAsString) {
  Buffer buf;
  buf.append("hello, world");
  EXPECT_EQ(buf.retrieveAsString(7), "hello, ");
  EXPECT_EQ(buf.readableBytes(), 5u);
  EXPECT_EQ(buf.retrieveAllAsString(), "world");
  EXPECT_EQ(buf.readableBytes(), 0u);
}

// 读写交替:readerIndex 前移后写入复用空间
TEST(BufferTest, ReadThenWriteReusesSpace) {
  Buffer buf;
  buf.append(std::string(100, 'a'));
  buf.retrieveAll();

  // retrieveAll 后可写空间应恢复完整 kInitialSize,而非累计增长
  EXPECT_EQ(buf.writeableBytes(), kInitialSize);

  buf.append("after");
  EXPECT_EQ(buf.retrieveAllAsString(), "after");
}

// ==================== findCRLF ====================
TEST(BufferTest, FindCRLF) {
  Buffer buf;
  buf.append("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n");

  const char* first = buf.findCRLF();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(std::distance(buf.peek(), first), 15);

  // 从 first+1 继续找,跳过第一个 CRLF
  const char* second = buf.findCRLF(first + 1);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(std::distance(buf.peek(), second), 34);

  // 尾部是 "\r\n\r\n":内含两个重叠 CRLF(偏移 34 和 36),继续找应命中第三个
  const char* third = buf.findCRLF(second + 1);
  ASSERT_NE(third, nullptr);
  EXPECT_EQ(std::distance(buf.peek(), third), 36);

  // 消费完最后一个 CRLF 后再找,应返回 nullptr
  EXPECT_EQ(buf.findCRLF(third + 2), nullptr);
}

TEST(BufferTest, FindCRLFNotFound) {
  Buffer buf;
  buf.append("no newline here");
  EXPECT_EQ(buf.findCRLF(), nullptr);
}

// ==================== retrieveUntil ====================
TEST(BufferTest, RetrieveUntil) {
  Buffer buf;
  buf.append("first\r\nsecond\r\n");

  const char* crlf = buf.findCRLF();
  ASSERT_NE(crlf, nullptr);
  buf.retrieveUntil(crlf);  // 消费到 CRLF 之前
  EXPECT_EQ(buf.retrieveAllAsString(), "\r\nsecond\r\n");
}

// ==================== 网络字节序整型编解码 ====================
TEST(BufferTest, IntegerRoundTrip) {
  Buffer buf;
  buf.appendInt32(0x01020304);
  buf.appendInt16(0x0506);
  buf.appendInt8(0x07);

  EXPECT_EQ(buf.readableBytes(), 7u);
  EXPECT_EQ(buf.readInt32(), 0x01020304);
  EXPECT_EQ(buf.readInt16(), 0x0506);
  EXPECT_EQ(buf.readInt_8(), 0x07);
  EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(BufferTest, IntegerPeekDoesNotConsume) {
  Buffer buf;
  buf.appendInt32(-1);  // 0xFFFFFFFF

  EXPECT_EQ(buf.peekInt32(), -1);
  EXPECT_EQ(buf.readableBytes(), 4u);  // peek 不消费
  EXPECT_EQ(buf.readInt32(), -1);
  EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(BufferTest, IntegerNegativeValues) {
  Buffer buf;
  buf.appendInt32(-12345);
  buf.appendInt16(-678);
  buf.appendInt8(-9);

  EXPECT_EQ(buf.readInt32(), -12345);
  EXPECT_EQ(buf.readInt16(), -678);
  EXPECT_EQ(buf.readInt_8(), -9);
}

// 网络字节序验证:appendInt32 写入的原始字节应为大端
TEST(BufferTest, IntegerBigEndianLayout) {
  Buffer buf;
  buf.appendInt32(0x01020304);
  const char* raw = buf.peek();
  EXPECT_EQ(raw[0], '\x01');
  EXPECT_EQ(raw[1], '\x02');
  EXPECT_EQ(raw[2], '\x03');
  EXPECT_EQ(raw[3], '\x04');
}

// ==================== prepend ====================
TEST(BufferTest, Prepend) {
  Buffer buf;
  buf.append("world");

  // 在已有数据前插头部
  buf.prepend("hello ", 6);
  EXPECT_EQ(buf.retrieveAllAsString(), "hello world");
}

TEST(BufferTest, PrependUsesPrependableSpace) {
  Buffer buf;
  EXPECT_EQ(buf.prependableBytes(), kCheapPrepend);

  // 初始状态下可以在预留区前插 kCheapPrepend 字节
  const char header[kCheapPrepend] = {1, 2, 3, 4, 5, 6, 7, 8};
  buf.prepend(header, kCheapPrepend);
  EXPECT_EQ(buf.prependableBytes(), 0u);
  EXPECT_EQ(buf.readableBytes(), kCheapPrepend);
  EXPECT_EQ(buf.peek()[0], '\x1');

  // 消费后空间归还,可再次 prepend
  buf.retrieveAll();
  EXPECT_EQ(buf.prependableBytes(), kCheapPrepend);
}

// ==================== 扩容 ====================
TEST(BufferTest, EnsureWriteableGrows) {
  Buffer buf;
  const std::string big(kInitialSize + 2000, 'x');
  buf.append(big);  // 超过初始容量,触发 makeSpace resize 分支

  EXPECT_EQ(buf.readableBytes(), big.size());
  EXPECT_EQ(buf.retrieveAllAsString(), big);
}

TEST(BufferTest, GrowthPreservesUnreadData) {
  Buffer buf;
  buf.append("keep me");
  // 再塞入远超剩余空间的数据,扩容时不能破坏未读数据
  buf.append(std::string(kInitialSize, 'y'));
  EXPECT_EQ(buf.readableBytes(), 7u + kInitialSize);
  EXPECT_EQ(buf.retrieveAsString(7), "keep me");
  EXPECT_EQ(buf.retrieveAllAsString(), std::string(kInitialSize, 'y'));
}

// makeSpace 的另一分支:不 resize,而是把未读数据前移腾出尾部空间
TEST(BufferTest, MakeSpaceMovesUnreadData) {
  Buffer buf;
  // 填满缓冲区
  buf.append(std::string(kInitialSize, 'a'));
  // 读取大部分,留下少量未读数据,使 prependable 空间足够搬移
  buf.retrieve(kInitialSize - 10);
  EXPECT_EQ(buf.readableBytes(), 10u);

  // 需要超过剩余可写的空间,但可写+可前移空间足够,走数据前移分支
  buf.append(std::string(500, 'b'));

  EXPECT_EQ(buf.readableBytes(), 510u);
  EXPECT_EQ(buf.retrieveAsString(10), std::string(10, 'a'));
  EXPECT_EQ(buf.retrieveAllAsString(), std::string(500, 'b'));
}

// ==================== shrink ====================
TEST(BufferTest, Shrink) {
  Buffer buf;
  buf.append(std::string(kInitialSize + 1000, 'x'));  // 先撑大
  buf.retrieve(1000);
  const size_t readable = buf.readableBytes();

  buf.shrink(64);  // 收缩,保留 64 字节可写余量

  EXPECT_EQ(buf.readableBytes(), readable);
  EXPECT_GE(buf.writeableBytes(), 64u);
  // 收缩后容量不应再是扩容后的大块头(至少小于扩容时的需求)
  EXPECT_EQ(buf.retrieveAllAsString(), std::string(readable, 'x'));
}

// ==================== swap ====================
TEST(BufferTest, Swap) {
  Buffer buf1;
  buf1.append("one");
  Buffer buf2;
  buf2.append("two", 3);
  buf2.retrieveAll();
  buf2.append(std::string(kInitialSize * 2, 'z'));

  buf1.swap(buf2);

  EXPECT_EQ(buf1.retrieveAllAsString(), std::string(kInitialSize * 2, 'z'));
  EXPECT_EQ(buf2.retrieveAllAsString(), "one");
}

// ==================== hasWritten / beginWrite ====================
TEST(BufferTest, HasWritten) {
  Buffer buf;
  buf.ensureWriteableBytes(100);
  std::memcpy(buf.beginWrite(), "abc", 3);
  buf.hasWritten(3);  // 模拟 readFd/writev 式的外部写入

  EXPECT_EQ(buf.readableBytes(), 3u);
  EXPECT_EQ(buf.retrieveAllAsString(), "abc");
}

// ==================== readFd ====================
TEST(BufferTest, ReadFdSmallData) {
  PipeFd pipe = PipeFd::createNonblocking();
  ASSERT_GE(pipe.readFd, 0);

  const std::string msg = "via readv";
  ASSERT_EQ(::write(pipe.writeFd, msg.data(), msg.size()),
            static_cast<ssize_t>(msg.size()));

  Buffer buf;
  int savedErrno = 0;
  const ssize_t n = buf.readFd(pipe.readFd, &savedErrno);

  EXPECT_EQ(n, static_cast<ssize_t>(msg.size()));
  EXPECT_EQ(savedErrno, 0);
  EXPECT_EQ(buf.retrieveAllAsString(), msg);
}

TEST(BufferTest, ReadFdOverflowIntoExtraBuffer) {
  // 写入超过 Buffer 可写空间的数据,迫使 readv 使用第二块 extrabuff,
  // 验证溢出部分被正确 append 回主缓冲区
  PipeFd pipe = PipeFd::createNonblocking();
  ASSERT_GE(pipe.readFd, 0);

  const size_t payloadSize = kInitialSize + 4096;
  const std::string payload(payloadSize, 'R');
  ASSERT_EQ(::write(pipe.writeFd, payload.data(), payload.size()),
            static_cast<ssize_t>(payload.size()));

  Buffer buf;
  int savedErrno = 0;
  const ssize_t n = buf.readFd(pipe.readFd, &savedErrno);

  // pipe 缓冲区(默认 64K)足以容纳 payload,一次 readv 应读全
  ASSERT_EQ(n, static_cast<ssize_t>(payloadSize));
  EXPECT_EQ(buf.readableBytes(), payloadSize);
  EXPECT_EQ(buf.retrieveAllAsString(), payload);
}

TEST(BufferTest, ReadFdWouldBlock) {
  PipeFd pipe = PipeFd::createNonblocking();
  ASSERT_GE(pipe.readFd, 0);

  Buffer buf;
  int savedErrno = 0;
  const ssize_t n = buf.readFd(pipe.readFd, &savedErrno);

  EXPECT_EQ(n, -1);
  EXPECT_EQ(savedErrno, EAGAIN);
  EXPECT_EQ(buf.readableBytes(), 0u);
}

// ==================== ensureWriteableBytes ====================
TEST(BufferTest, EnsureWriteableNoOpWhenSufficient) {
  Buffer buf;
  const size_t before = buf.writeableBytes();
  buf.ensureWriteableBytes(before);  // 恰好够,不应扩容
  EXPECT_EQ(buf.writeableBytes(), before);
}

TEST(BufferTest, EnsureWriteableGrowsWhenInsufficient) {
  Buffer buf;
  buf.ensureWriteableBytes(kInitialSize + 1);
  EXPECT_GE(buf.writeableBytes(), kInitialSize + 1);
}
