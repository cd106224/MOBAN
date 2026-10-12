#include <base/timestamp.h>
#include <sys/time.h>

#include <cinttypes>

Timestamp::Timestamp() : micro_seconds_since_epoch_(0) {}

Timestamp::Timestamp(int64_t microseconds)
    : micro_seconds_since_epoch_(microseconds) {}

Timestamp::~Timestamp() = default;

std::string Timestamp::to_string() const {
  char buf[32] = {0};
  int64_t seconds = micro_seconds_since_epoch_ / kMicroSecondsPerSecond;
  int64_t microseconds = micro_seconds_since_epoch_ % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf) - 1, "%" PRId64 ".%06" PRId64 "", seconds,
           microseconds);
  return buf;
}

void Timestamp::swap(Timestamp& that) {
  std::swap(micro_seconds_since_epoch_, that.micro_seconds_since_epoch_);
}

std::string Timestamp::to_formatted_string() const {
  // 32 is enough at runtime, but GCC -Wformat-truncation assumes int args can
  // be up to 11 chars each (worst case ~78 bytes) and errors out under -Werror.
  char buf[80] = {0};
  auto seconds =
      static_cast<time_t>(micro_seconds_since_epoch_ / kMicroSecondsPerSecond);
  int microseconds =
      static_cast<int>(micro_seconds_since_epoch_ % kMicroSecondsPerSecond);
  struct tm tm_time{};
  gmtime_r(&seconds, &tm_time);

  snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
           tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
           tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, microseconds);
  return buf;
}

bool Timestamp::valid() const { return micro_seconds_since_epoch_ > 0; }

int64_t Timestamp::micro_seconds_since_epoch() const {
  return micro_seconds_since_epoch_;
}

time_t Timestamp::seconds_since_epoch() const {
  return static_cast<time_t>(micro_seconds_since_epoch_ /
                             kMicroSecondsPerSecond);
}

Timestamp Timestamp::now() {
  struct timeval tv{};
  gettimeofday(&tv, nullptr);
  int64_t seconds = tv.tv_sec;
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

Timestamp Timestamp::invalid() { return {}; }
