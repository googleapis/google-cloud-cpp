// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

std::vector<std::string> CaptureLogLinesBackend::ClearLogLines() {
  std::vector<std::string> result;
  {
    std::lock_guard<std::mutex> lk(mu_);
    result.swap(log_lines_);
  }
  return result;
}

void CaptureLogLinesBackend::Process(LogRecord const& lr) {
  // Break the records in lines, it is easier to analyze them as such.
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<std::string> result = absl::StrSplit(lr.message, '\n');
  log_lines_.insert(log_lines_.end(), result.begin(), result.end());
}

void CaptureLogLinesBackend::ProcessWithOwnership(LogRecord lr) { Process(lr); }

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
