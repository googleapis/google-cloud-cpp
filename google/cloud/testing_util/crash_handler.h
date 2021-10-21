// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CRASH_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CRASH_HANDLER_H

#include "google/cloud/version.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * Installs the symbolizer and signal handler from Abseil for this process.
 *
 * This function should only be called once in any process. It should be called
 * from `main()` so it can be passed the path to the binary, `argv[0]`.
 *
 * @see
 * https://github.com/abseil/abseil-cpp/blob/master/absl/debugging/symbolize.h
 * @see
 * https://github.com/abseil/abseil-cpp/blob/master/absl/debugging/failure_signal_handler.h
 */
void InstallCrashHandler(char const* argv0);

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CRASH_HANDLER_H
