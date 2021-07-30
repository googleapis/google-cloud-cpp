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

#include "google/cloud/internal/log_impl.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

void StdClogBackend::Process(LogRecord const& lr) {
  std::lock_guard<std::mutex> lk(mu_);
  if (lr.severity >= min_severity_) {
    std::clog << lr << "\n";
    if (lr.severity >= Severity::GCP_LS_WARNING) {
      std::clog << std::flush;
    }
  }
}

void StdClogBackend::Flush() { std::clog << std::flush; }

void CircularBufferBackend::ProcessWithOwnership(LogRecord lr) {
  std::unique_lock<std::mutex> lk(mu_);
  auto const needs_flush = lr.severity >= min_flush_severity_;
  buffer_[index(end_)] = std::move(lr);
  ++end_;
  if (end_ - begin_ > buffer_.size()) ++begin_;
  if (needs_flush) FlushImpl(std::move(lk));
}

void CircularBufferBackend::Flush() {
  FlushImpl(std::unique_lock<std::mutex>(mu_));
}

void CircularBufferBackend::FlushImpl(std::unique_lock<std::mutex> /*lk*/) {
  for (auto i = begin_; i != end_; ++i) {
    backend_->ProcessWithOwnership(std::move(buffer_[index(i)]));
  }
  begin_ = end_ = 0;
  backend_->Flush();
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
