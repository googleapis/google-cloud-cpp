// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/log.h"
#include "google/cloud/internal/getenv.h"
#include <array>
#include <cstdint>
#include <ctime>
#include <iomanip>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

static_assert(sizeof(Severity) <= sizeof(int),
              "Expected Severity to fit in an integer");

static_assert(static_cast<int>(Severity::GCP_LS_LOWEST) <
                  static_cast<int>(Severity::GCP_LS_HIGHEST),
              "Expect LOWEST severity to be smaller than HIGHEST severity");

namespace {
struct Timestamp {
  explicit Timestamp(std::chrono::system_clock::time_point const& tp) {
    auto const tt = std::chrono::system_clock::to_time_t(tp);
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    auto const ss = tp - std::chrono::system_clock::from_time_t(tt);
    nanos = static_cast<std::int32_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(ss).count());
  }
  std::tm tm;
  std::int32_t nanos;
};

std::ostream& operator<<(std::ostream& os, Timestamp const& ts) {
  auto const prev = os.fill(' ');
  auto constexpr kTmYearOffset = 1900;
  os << std::setw(4) << ts.tm.tm_year + kTmYearOffset;
  os.fill('0');
  os << '-' << std::setw(2) << ts.tm.tm_mon + 1;
  os << '-' << std::setw(2) << ts.tm.tm_mday;
  os << 'T' << std::setw(2) << ts.tm.tm_hour;
  os << ':' << std::setw(2) << ts.tm.tm_min;
  os << ':' << std::setw(2) << ts.tm.tm_sec;
  // NOLINTNEXTLINE(readability-magic-numbers)
  os << '.' << std::setw(9) << ts.nanos << 'Z';
  os.fill(prev);
  return os;
}

auto constexpr kSeverityCount = static_cast<int>(Severity::GCP_LS_HIGHEST) + 1;

// Double braces needed to workaround a clang-3.8 bug.
std::array<char const*, kSeverityCount> constexpr kSeverityNames{{
    "TRACE",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARNING",
    "ERROR",
    "CRITICAL",
    "ALERT",
    "FATAL",
}};
}  // namespace

std::ostream& operator<<(std::ostream& os, Severity x) {
  auto index = static_cast<int>(x);
  return os << kSeverityNames[index];
}

std::ostream& operator<<(std::ostream& os, LogRecord const& rhs) {
  return os << Timestamp{rhs.timestamp} << " [" << rhs.severity << "] "
            << rhs.message << " (" << rhs.filename << ':' << rhs.lineno << ')';
}

LogSink::LogSink()
    : empty_(true),
      minimum_severity_(static_cast<int>(Severity::GCP_LS_LOWEST_ENABLED)),
      next_id_(0),
      clog_backend_id_(0) {}

LogSink& LogSink::Instance() {
  static auto* const kInstance = [] {
    auto* p = new LogSink;
    if (internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_CLOG").has_value()) {
      p->EnableStdClogImpl();
    }
    return p;
  }();
  return *kInstance;
}

// NOLINTNEXTLINE(google-runtime-int)
long LogSink::AddBackend(std::shared_ptr<LogBackend> backend) {
  std::unique_lock<std::mutex> lk(mu_);
  return AddBackendImpl(std::move(backend));
}

// NOLINTNEXTLINE(google-runtime-int)
void LogSink::RemoveBackend(long id) {
  std::unique_lock<std::mutex> lk(mu_);
  RemoveBackendImpl(id);
}

void LogSink::ClearBackends() {
  std::unique_lock<std::mutex> lk(mu_);
  backends_.clear();
  clog_backend_id_ = 0;
  empty_.store(backends_.empty());
}

std::size_t LogSink::BackendCount() const {
  std::unique_lock<std::mutex> lk(mu_);
  return backends_.size();
}

void LogSink::Log(LogRecord log_record) {
  // Make a copy of the backends because calling user-defined functions while
  // holding a lock is a bad idea: the application may change the backends while
  // we are holding this lock, and soon deadlock occurs.
  auto copy = [this]() {
    std::unique_lock<std::mutex> lk(mu_);
    return backends_;
  }();
  if (copy.empty()) {
    return;
  }
  // In general, we just give each backend a const-reference and the backends
  // must make a copy if needed.  But if there is only one backend we can give
  // the backend an opportunity to optimize things by transferring ownership of
  // the LogRecord to it.
  if (copy.size() == 1) {
    copy.begin()->second->ProcessWithOwnership(std::move(log_record));
    return;
  }
  for (auto& kv : copy) {
    kv.second->Process(log_record);
  }
}

namespace {
class StdClogBackend : public LogBackend {
 public:
  StdClogBackend() = default;

  void Process(LogRecord const& lr) override {
    std::clog << lr << "\n";
    if (lr.severity >= Severity::GCP_LS_WARNING) {
      std::clog << std::flush;
    }
  }
  void ProcessWithOwnership(LogRecord lr) override { Process(lr); }
};
}  // namespace

void LogSink::EnableStdClogImpl() {
  std::unique_lock<std::mutex> lk(mu_);
  if (clog_backend_id_ != 0) {
    return;
  }
  clog_backend_id_ = AddBackendImpl(std::make_shared<StdClogBackend>());
}

void LogSink::DisableStdClogImpl() {
  std::unique_lock<std::mutex> lk(mu_);
  if (clog_backend_id_ == 0) {
    return;
  }
  RemoveBackendImpl(clog_backend_id_);
  clog_backend_id_ = 0;
}

// NOLINTNEXTLINE(google-runtime-int)
long LogSink::AddBackendImpl(std::shared_ptr<LogBackend> backend) {
  auto const id = ++next_id_;
  backends_.emplace(id, std::move(backend));
  empty_.store(backends_.empty());
  return id;
}

// NOLINTNEXTLINE(google-runtime-int)
void LogSink::RemoveBackendImpl(long id) {
  auto it = backends_.find(id);
  if (backends_.end() == it) {
    return;
  }
  backends_.erase(it);
  empty_.store(backends_.empty());
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
