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
LogSink::LogSink()
    : empty_(true),
      minimum_severity_(static_cast<int>(Severity::LOWEST_ENABLED)),
      next_id_(0) {}
      
LogSink& LogSink::Instance() {
  static LogSink instance;
  return instance;
}

long LogSink::AddBackend(std::shared_ptr<LogBackend> backend) {
  std::unique_lock<std::mutex> lk(mu_);
  long id = ++next_id_;
  backends_.emplace(id, std::move(backend));
  empty_.store(backends_.empty());
  return id;
}

void LogSink::RemoveBackend(long id) {
  std::unique_lock<std::mutex> lk(mu_);
  auto it = backends_.find(id);
  if (backends_.end() == it) {
    return;
  }
  backends_.erase(it);
  empty_.store(backends_.empty());
}

void LogSink::ClearBackends() {
  std::unique_lock<std::mutex> lk(mu_);
  backends_.clear();
  empty_.store(backends_.empty());
}

std::ostream& operator<<(std::ostream& os, Severity x) {
  char const* names[] = {
      "TRACE", "DEBUG",    "INFO",  "NOTICE", "WARNING",
      "ERROR", "CRITICAL", "ALERT", "FATAL",
  };
  auto index = static_cast<int>(x);
  return os << names[index];
}
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
