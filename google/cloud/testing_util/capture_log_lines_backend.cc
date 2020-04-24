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

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

void CaptureLogLinesBackend::Process(LogRecord const& lr) {
  // Break the records in lines, it is easier to analyze them as such.
  std::istringstream is(lr.message);
  std::string line;
  while (std::getline(is, line)) {
    log_lines.emplace_back(line);
  }
}

void CaptureLogLinesBackend::ProcessWithOwnership(LogRecord lr) { Process(lr); }

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
