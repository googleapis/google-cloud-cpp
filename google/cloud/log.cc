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

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
static_assert(sizeof(Severity) <= sizeof(int),
              "Expected Severity to fit in an integer");

static_assert(static_cast<int>(Severity::GCP_LS_LOWEST) <
                  static_cast<int>(Severity::GCP_LS_HIGHEST),
              "Expect LOWEST severity to be smaller than HIGHEST severity");

std::ostream& operator<<(std::ostream& os, Severity x) {
  char const* names[] = {
      "TRACE", "DEBUG",    "INFO",  "NOTICE", "WARNING",
      "ERROR", "CRITICAL", "ALERT", "FATAL",
  };
  auto index = static_cast<int>(x);
  return os << names[index];
}

std::ostream& operator<<(std::ostream& os, LogRecord const& rhs) {
  return os << '[' << rhs.severity << "] " << rhs.message << " ("
            << rhs.filename << ':' << rhs.lineno << ')';
}

LogSink::LogSink()
    : empty_(true),
      minimum_severity_(static_cast<int>(Severity::GCP_LS_LOWEST_ENABLED)),
      next_id_(0),
      clog_backend_id_(0) {}

LogSink& LogSink::Instance() {
  static LogSink instance;
  return instance;
}

long LogSink::AddBackend(std::shared_ptr<LogBackend> backend) {
  std::unique_lock<std::mutex> lk(mu_);
  return AddBackendImpl(std::move(backend));
}

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
  if (1U == copy.size()) {
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

long LogSink::AddBackendImpl(std::shared_ptr<LogBackend> backend) {
  long id = ++next_id_;
  backends_.emplace(id, std::move(backend));
  empty_.store(backends_.empty());
  return id;
}

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
