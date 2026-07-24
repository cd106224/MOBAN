#include <base/timestamp.h>
#include <sys/time.h>

#include <cinttypes>

Timestamp::Timestamp() : m_microSecondsSinceEpoch(0) {}

Timestamp::Timestamp(int64_t microseconds)
    : m_microSecondsSinceEpoch(microseconds) {}

Timestamp::~Timestamp() = default;

std::string Timestamp::toString() const {
  char buf[32] = {0};
  int64_t seconds = m_microSecondsSinceEpoch / kMicroSecondsPerSecond;
  int64_t microseconds = m_microSecondsSinceEpoch % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf) - 1, "%" PRId64 ".%06" PRId64 "", seconds,
           microseconds);
  return buf;
}

void Timestamp::swap(Timestamp &that) {
  std::swap(m_microSecondsSinceEpoch, that.m_microSecondsSinceEpoch);
}

std::string Timestamp::toFormattedString() const {
  char buf[32] = {0};
  auto seconds =
      static_cast<time_t>(m_microSecondsSinceEpoch / kMicroSecondsPerSecond);
  int microseconds =
      static_cast<int>(m_microSecondsSinceEpoch % kMicroSecondsPerSecond);
  struct tm tm_time{};
  gmtime_r(&seconds, &tm_time);

  snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
           tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
           tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, microseconds);
  return buf;
}

bool Timestamp::valid() const { return m_microSecondsSinceEpoch > 0; }

int64_t Timestamp::microSecondsSinceEpoch() const {
  return m_microSecondsSinceEpoch;
}

time_t Timestamp::secondsSicneEpoch() const {
  return static_cast<time_t>(m_microSecondsSinceEpoch / kMicroSecondsPerSecond);
}

Timestamp Timestamp::now() {
  struct timeval tv{};
  gettimeofday(&tv, nullptr);
  int64_t seconds = tv.tv_sec;
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

Timestamp Timestamp::invalid() { return {}; }
