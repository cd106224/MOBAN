#include <unistd.h>

#include <string>
#include <vector>

namespace Muduo {
class Buffer {
 public:
  static const size_t kCheapprepend = 8;
  static const size_t kInitialSize = 1024;

  Buffer();
  void swap(Buffer& rhs);
  size_t readableBytes();
  size_t writeableBytes();
  size_t prependableBytes();
  const char* peek();
  const char* findCRLF();
  const char* findCRLF(const char* start);
  void retrieve(size_t len);
  // 取回直到end
  void retrieveUntil(const char* end);
  void retrieveInt32();
  void retrieveInt16();
  void retrieveInt8();
  void retrieveAll();
  std::string retrieveAllAsString();
  std::string retrieveAsString(size_t len);
  void append(const std::string& str);
  void append(const char* data, size_t len);
  void append(const void* data, size_t len);
  // 确保缓冲区可写空间>=len,如果不足则扩充
  void ensureWriteableBytes(size_t len);
  char* beginWrite();
  const char* beginWrite() const;
  void hasWritten(size_t len);
  void appendInt32(int32_t x);
  void appendInt16(int16_t x);
  void appendInt8(int8_t x);
  int32_t readInt32();
  int16_t readInt16();
  int8_t readInt_8();
  int32_t peekInt32();
  int16_t peekInt16();
  int8_t peekInt8();
  void prependInt32(int32_t x);
  void prependInt16(int16_t x);
  void prependInt8(int8_t x);
  void prepend(const void* data, size_t len);
  // 收缩,保留reserve个字节
  void shrink(size_t reserve);
  ssize_t readFd(int fd, int* saveErrno);

 protected:
  char* begin();
  const char* begin() const;
  void makeSpace(size_t len);

 protected:
  std::vector<char> buffer_;
  size_t readerIndex_;  // 读位置
  size_t writeIndex_;   // 写位置
  static const char KCRLF[];
};
};  // namespace Muduo