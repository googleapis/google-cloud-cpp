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
#include "google/cloud/internal/log_impl.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include <array>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

static_assert(sizeof(Severity) == sizeof(int),
              "Expected Severity to be represented as an int");

static_assert(static_cast<int>(Severity::GCP_LS_LOWEST) <
                  static_cast<int>(Severity::GCP_LS_HIGHEST),
              "Expect LOWEST severity to be smaller than HIGHEST severity");

static_assert(static_cast<int>(Severity::GCP_LS_LOWEST_ENABLED) <=
                  static_cast<int>(Severity::GCP_LS_FATAL),
              "Severity FATAL cannot be disable at compile time");

namespace {
struct Timestamp {
  explicit Timestamp(std::chrono::system_clock::time_point const& tp)
      : t(absl::FromChrono(tp)) {}
  absl::Time t;
};

std::ostream& operator<<(std::ostream& os, Timestamp const& ts) {
  auto constexpr kFormat = "%E4Y-%m-%dT%H:%M:%E9SZ";
  return os << absl::FormatTime(kFormat, ts.t, absl::UTCTimeZone());
}

auto constexpr kSeverityCount = static_cast<int>(Severity::GCP_LS_HIGHEST) + 1;

std::array<char const*, kSeverityCount> constexpr kSeverityNames{
    "TRACE", "DEBUG",    "INFO",  "NOTICE", "WARNING",
    "ERROR", "CRITICAL", "ALERT", "FATAL",
};

absl::optional<Severity> ParseSeverity(std::string const& name) {
  int i = 0;
  for (auto const* n : kSeverityNames) {
    if (name == n) return static_cast<Severity>(i);
    ++i;
  }
  return {};
}

absl::optional<std::size_t> ParseSize(std::string const& str) {
  std::size_t econv = -1;
  auto const val = std::stol(str, &econv);
  if (econv != str.size()) return {};
  if (val <= 0) return {};
  return static_cast<std::size_t>(val);
}

}  // namespace

std::ostream& operator<<(std::ostream& os, Severity x) {
  auto index = static_cast<int>(x);
  return os << kSeverityNames[index];
}

std::ostream& operator<<(std::ostream& os, LogRecord const& rhs) {
  return os << Timestamp{rhs.timestamp} << " [" << rhs.severity << "]"
            << " <" << std::this_thread::get_id() << ">"
            << " " << rhs.message << " (" << rhs.filename << ':' << rhs.lineno
            << ')';
}

LogSink::LogSink()
    : empty_(true),
      minimum_severity_(static_cast<int>(Severity::GCP_LS_LOWEST_ENABLED)) {}

LogSink& LogSink::Instance() {
  static auto* const kInstance = [] {
    auto* p = new LogSink;
    p->SetDefaultBackend(internal::DefaultLogBackend());
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
  default_backend_id_ = 0;
  empty_.store(backends_.empty());
}

std::size_t LogSink::BackendCount() const {
  std::unique_lock<std::mutex> lk(mu_);
  return backends_.size();
}

void LogSink::Log(LogRecord log_record) {
  auto copy = CopyBackends();
  if (copy.empty()) return;
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

void LogSink::Flush() {
  auto copy = CopyBackends();
  for (auto& kv : copy) kv.second->Flush();
}

void LogSink::EnableStdClogImpl(Severity min_severity) {
  std::unique_lock<std::mutex> lk(mu_);
  if (default_backend_id_ != 0) return;
  default_backend_id_ =
      AddBackendImpl(std::make_shared<internal::StdClogBackend>(min_severity));
}

void LogSink::DisableStdClogImpl() {
  std::unique_lock<std::mutex> lk(mu_);
  if (default_backend_id_ == 0) return;
  // Note that the backend set by SetDefaultBackend() may be any LogBackend
  // subclass, and so not necessarily a StdClogBackend. But, by default, it
  // always is one, or a CircularBufferBackend that wraps a StdClogBackend.
  RemoveBackendImpl(default_backend_id_);
  default_backend_id_ = 0;
}

void LogSink::SetDefaultBackend(std::shared_ptr<LogBackend> backend) {
  std::unique_lock<std::mutex> lk(mu_);
  if (default_backend_id_ != 0) return;
  default_backend_id_ = AddBackendImpl(std::move(backend));
}

LogSink::BackendId LogSink::AddBackendImpl(
    std::shared_ptr<LogBackend> backend) {
  auto const id = ++next_id_;
  backends_.emplace(id, std::move(backend));
  empty_.store(backends_.empty());
  return id;
}

void LogSink::RemoveBackendImpl(BackendId id) {
  auto it = backends_.find(id);
  if (backends_.end() == it) {
    return;
  }
  backends_.erase(it);
  empty_.store(backends_.empty());
}

// Make a copy of the backends because calling user-defined functions while
// holding a lock is a bad idea: the application may change the backends while
// we are holding this lock, and soon deadlock occurs.
std::map<LogSink::BackendId, std::shared_ptr<LogBackend>>
LogSink::CopyBackends() {
  std::lock_guard<std::mutex> lk(mu_);
  return backends_;
}

namespace internal {

std::shared_ptr<LogBackend> DefaultLogBackend() {
  auto constexpr kLogConfig = "GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG";
  auto constexpr kEnableClog = "GOOGLE_CLOUD_CPP_ENABLE_CLOG";

  auto config = internal::GetEnv(kLogConfig).value_or("");
  std::vector<std::string> fields = absl::StrSplit(config, ',');
  if (!fields.empty()) {
    auto min_severity = Severity::GCP_LS_LOWEST_ENABLED;
    if (fields[0] == "lastN" && fields.size() == 3) {
      auto size = ParseSize(fields[1]);
      auto min_flush_severity = ParseSeverity(fields[2]);
      if (size.has_value() && min_flush_severity.has_value()) {
        return std::make_shared<CircularBufferBackend>(
            *size, *min_flush_severity,
            std::make_shared<StdClogBackend>(min_severity));
      }
    }
    if (fields[0] == "clog" && fields.size() == 1) {
      return std::make_shared<StdClogBackend>(min_severity);
    }
  }

  auto min_severity =
      ParseSeverity(internal::GetEnv(kEnableClog).value_or("FATAL"));
  return std::make_shared<StdClogBackend>(
      min_severity.value_or(Severity::GCP_LS_LOWEST_ENABLED));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
