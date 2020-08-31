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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CAPTURE_LOG_LINES_BACKEND_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CAPTURE_LOG_LINES_BACKEND_H

#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * A log backend that stores all the log lines.
 *
 * This is useful in tests that want to verify specific messages are logged.
 */
class CaptureLogLinesBackend : public LogBackend {
 public:
  std::vector<std::string> ClearLogLines();

  void Process(LogRecord const& lr) override;
  void ProcessWithOwnership(LogRecord lr) override;

 private:
  std::mutex mu_;
  std::vector<std::string> log_lines_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CAPTURE_LOG_LINES_BACKEND_H
