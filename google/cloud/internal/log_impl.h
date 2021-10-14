// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_IMPL_H

#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include <algorithm>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class StdClogBackend : public LogBackend {
 public:
  explicit StdClogBackend(Severity min_severity)
      : min_severity_((std::min)(min_severity, Severity::GCP_LS_FATAL)) {}

  Severity min_severity() const { return min_severity_; }

  void Process(LogRecord const& lr) override;
  void ProcessWithOwnership(LogRecord lr) override { Process(lr); }
  void Flush() override;

 private:
  std::mutex mu_;
  Severity min_severity_;
};

class CircularBufferBackend : public LogBackend {
 public:
  CircularBufferBackend(std::size_t size, Severity min_flush_severity,
                        std::shared_ptr<LogBackend> backend)
      : buffer_(size),
        begin_(0),
        end_(0),
        min_flush_severity_(min_flush_severity),
        backend_(std::move(backend)) {}

  std::size_t size() const { return buffer_.size(); }
  Severity min_flush_severity() const { return min_flush_severity_; }
  std::shared_ptr<LogBackend> backend() const { return backend_; }

  void Process(LogRecord const& lr) override { ProcessWithOwnership(lr); }
  void ProcessWithOwnership(LogRecord lr) override;
  void Flush() override;

 private:
  std::size_t index(std::size_t i) const { return i % buffer_.size(); }

  void FlushImpl(std::unique_lock<std::mutex> lk);

  std::mutex mu_;
  std::vector<LogRecord> buffer_;
  std::size_t begin_;
  std::size_t end_;
  Severity min_flush_severity_;
  std::shared_ptr<LogBackend> backend_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_IMPL_H
