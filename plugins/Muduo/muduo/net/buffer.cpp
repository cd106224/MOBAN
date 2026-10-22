#include "buffer.h"

#include <netinet/in.h>
#include <sys/uio.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

namespace Muduo {

const char Buffer::KCRLF[] = "\r\n";

Buffer::Buffer()
    : buffer_(kCheapprepend + kInitialSize),
      readerIndex_(kCheapprepend),
      writeIndex_(kCheapprepend) {
  assert(readableBytes() == 0);
  assert(writeableBytes() == kInitialSize);
  assert(prependableBytes() == kCheapprepend);
}

void Buffer::swap(Buffer& rhs) {
  buffer_.swap(rhs.buffer_);
  std::swap(readerIndex_, rhs.readerIndex_);
  std::swap(writeIndex_, rhs.writeIndex_);
}

size_t Buffer::readableBytes() { return writeIndex_ - readerIndex_; }

size_t Buffer::writeableBytes() { return buffer_.size() - writeIndex_; }

size_t Buffer::prependableBytes() { return readerIndex_; }

const char* Buffer::peek() { return begin() + readerIndex_; }

const char* Buffer::findCRLF() {
  const char* crlf = std::search(peek(), static_cast<const char*>(beginWrite()),
                                 KCRLF, KCRLF + 2);
  return crlf == beginWrite() ? nullptr : crlf;
}

const char* Buffer::findCRLF(const char* start) {
  assert(peek() <= start);
  assert(start <= beginWrite());
  const char* crlf = std::search(start, static_cast<const char*>(beginWrite()),
                                 KCRLF, KCRLF + 2);
  return crlf == beginWrite() ? nullptr : crlf;
}

void Buffer::retrieve(size_t len) {
  assert(len <= readableBytes());
  if (len < readableBytes()) {
    readerIndex_ += len;
  } else {
    retrieveAll();
  }
}

void Buffer::retrieveUntil(const char* end) {
  assert(peek() <= end);
  assert(end <= beginWrite());
  retrieve(static_cast<size_t>(end - peek()));
}

void Buffer::retrieveInt32() { retrieve(sizeof(int32_t)); }

void Buffer::retrieveInt16() { retrieve(sizeof(int16_t)); }

void Buffer::retrieveInt8() { retrieve(sizeof(int8_t)); }

void Buffer::retrieveAll() {
  readerIndex_ = kCheapprepend;
  writeIndex_ = kCheapprepend;
}

std::string Buffer::retrieveAllAsString() {
  return retrieveAsString(readableBytes());
}

std::string Buffer::retrieveAsString(size_t len) {
  assert(len <= readableBytes());
  std::string result(peek(), len);
  retrieve(len);
  return result;
}

void Buffer::append(const std::string& str) { append(str.data(), str.size()); }

void Buffer::append(const char* data, size_t len) {
  ensureWriteableBytes(len);
  std::copy(data, data + len, beginWrite());
  hasWritten(len);
}

void Buffer::append(const void* data, size_t len) {
  append(static_cast<const char*>(data), len);
}

void Buffer::ensureWriteableBytes(size_t len) {
  if (writeableBytes() < len) {
    makeSpace(len);
  }
  assert(writeableBytes() >= len);
}

char* Buffer::beginWrite() { return begin() + writeIndex_; }

const char* Buffer::beginWrite() const { return begin() + writeIndex_; }

void Buffer::hasWritten(size_t len) { writeIndex_ += len; }

void Buffer::appendInt32(int32_t x) {
  auto be32 = htonl(static_cast<uint32_t>(x));
  append(&be32, sizeof(int32_t));
}

void Buffer::appendInt16(int16_t x) {
  auto be16 = htons(static_cast<uint16_t>(x));
  append(&be16, sizeof(int16_t));
}

void Buffer::appendInt8(int8_t x) { append(&x, sizeof(int8_t)); }

int32_t Buffer::readInt32() {
  int32_t result = peekInt32();
  retrieveInt32();
  return result;
}

int16_t Buffer::readInt16() {
  int16_t result = peekInt16();
  retrieveInt16();
  return result;
}

int8_t Buffer::readInt_8() {
  int8_t result = peekInt8();
  retrieveInt8();
  return result;
}

int32_t Buffer::peekInt32() {
  assert(readableBytes() >= sizeof(int32_t));
  uint32_t be32 = 0;
  memcpy(&be32, peek(), sizeof(be32));
  return static_cast<int32_t>(ntohl(be32));
}

int16_t Buffer::peekInt16() {
  assert(readableBytes() >= sizeof(int16_t));
  uint16_t be16 = 0;
  memcpy(&be16, peek(), sizeof(be16));
  return static_cast<int16_t>(ntohs(be16));
}

int8_t Buffer::peekInt8() {
  assert(readableBytes() >= sizeof(int8_t));
  int8_t x = *peek();
  return x;
}

void Buffer::prependInt32(int32_t x) {
  auto be32 = htonl(static_cast<uint32_t>(x));
  prepend(&be32, sizeof(be32));
}

void Buffer::prependInt16(int16_t x) {
  auto be16 = htons(static_cast<uint16_t>(x));
  prepend(&be16, sizeof(be16));
}

void Buffer::prependInt8(int8_t x) { prepend(&x, sizeof(int8_t)); }

void Buffer::prepend(const void* data, size_t len) {
  assert(len <= prependableBytes());
  readerIndex_ -= len;
  const char* d = static_cast<const char*>(data);
  std::copy(d, d + len, begin() + readerIndex_);
}

void Buffer::shrink(size_t reserve) {
  Buffer other;
  other.ensureWriteableBytes(readableBytes() + reserve);
  other.append(retrieveAllAsString());
  swap(other);
}

char* Buffer::begin() { return &*buffer_.begin(); }

const char* Buffer::begin() const { return &*buffer_.begin(); }

// 结合栈上的空间，避免内存使用过大，提高内存使用率
// 如果有5K个连接，每个连接就分配64K+64K的缓冲区的话，将占用640M内存，
// 而大多数时候，这些缓冲区的使用率很低
ssize_t Buffer::readFd(int fd, int* saveErrno) {
  char extrabuff[65536]{};
  struct iovec vec[2];
  const size_t writable = writeableBytes();
  // 第一块缓冲区
  vec[0].iov_base = begin() + writeIndex_;
  vec[0].iov_len = writable;
  // 第二块缓冲区
  vec[1].iov_base = extrabuff;
  vec[1].iov_len = sizeof(extrabuff);

  const ssize_t n = ::readv(fd, vec, 2);
  if (n < 0) {
    *saveErrno = errno;
  } else if (static_cast<size_t>(n) <= writable) {
    writeIndex_ += static_cast<size_t>(n);
  } else {
    // 当前缓冲区,不能容纳，因此数据被接收到了第二块缓冲区，将其append到buffer_
    writeIndex_ = buffer_.size();
    append(extrabuff, static_cast<size_t>(n) - writable);
  }
  return n;
}

void Buffer::makeSpace(size_t len) {
  if (writeableBytes() + prependableBytes() < len + kCheapprepend) {
    // resize or reserve
    buffer_.resize(writeIndex_ + len);
  } else {
    assert(kCheapprepend < readerIndex_);
    size_t readable = readableBytes();
    std::copy(begin() + readerIndex_, begin() + writeIndex_,
              begin() + kCheapprepend);
    readerIndex_ = kCheapprepend;
    writeIndex_ = readerIndex_ + readable;
    assert(readableBytes() == readable);
  }
}
};  // namespace Muduo