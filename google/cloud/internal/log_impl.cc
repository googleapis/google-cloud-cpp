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
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

void StdClogBackend::Process(LogRecord const& lr) {
  std::lock_guard<std::mutex> lk(mu_);
  std::clog << lr << "\n";
  if (lr.severity >= Severity::GCP_LS_WARNING) {
    std::clog << std::flush;
  }
}

void CircularBufferBackend::ProcessWithOwnership(LogRecord lr) {
  std::lock_guard<std::mutex> lk(mu_);
  if (lr.severity >= min_flush_severity_) {
    Flush(std::move(lr));
    return;
  }
  buffer_[end_] = std::move(lr);
  end_ = increment(end_);
  if (end_ == begin_) begin_ = increment(begin_);
}

void CircularBufferBackend::Flush(LogRecord lr) {
  for (auto e = begin_; e != end_; e = increment(e)) {
    backend_->ProcessWithOwnership(std::move(buffer_[e]));
  }
  backend_->ProcessWithOwnership(std::move(lr));
  begin_ = end_ = 0;
}

namespace {
std::shared_ptr<LogBackend> LegacyLogBackend() {
  if (internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_CLOG").has_value()) {
    return std::make_shared<StdClogBackend>();
  }
  return {};
}


}  // namespace

std::shared_ptr<LogBackend> DefaultLogBackend() {
  auto config = internal::GetEnv("GOOGLE_CLOUD_CPP_LOG_CONFIG").value_or("");
  std::vector<std::string> fields = absl::StrSplit(config, ',');
  if (fields.empty()) return LegacyLogBackend();
  if (fields[0] == "clog") return std::make_shared<StdClogBackend>();
  if (fields[0] == "lastN") {

  }
  if (config.rfind("clog,", 0) == 0) {
    auto value =
  }
  return LegacyLogBackend();
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
