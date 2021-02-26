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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_LOG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_LOG_H

#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * Captures log lines within the current scope.
 *
 * Captured lines are exposed via the `ScopedLog::ExtractLines` method.
 *
 * @par Example
 * @code
 * TEST(Foo, Bar) {
 *   ScopedLog log;
 *   ... call code that should log
 *   EXPECT_THAT(log.ExtractLines(), testing::Contains("foo"));
 * }
 * @endcode
 */
class ScopedLog {
 public:
  ScopedLog()
      : backend_(std::make_shared<Backend>()),
        id_(LogSink::Instance().AddBackend(backend_)) {}
  ~ScopedLog() { LogSink::Instance().RemoveBackend(id_); }

  std::vector<std::string> ExtractLines() { return backend_->ExtractLines(); }

 private:
  class Backend : public LogBackend {
   public:
    std::vector<std::string> ExtractLines();
    void Process(LogRecord const& lr) override;
    void ProcessWithOwnership(LogRecord lr) override;

   private:
    std::mutex mu_;
    std::vector<std::string> log_lines_;
  };

  std::shared_ptr<Backend> backend_;
  long id_;  // NOLINT(google-runtime-int)
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_LOG_H
