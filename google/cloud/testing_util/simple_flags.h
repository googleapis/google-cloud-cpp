// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SIMPLE_FLAGS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SIMPLE_FLAGS_H_

#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include <algorithm>
#include <atomic>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * An extension point to parse flags of different types.
 *
 * If the code fails to compile it is because you need to specialize this
 * function.
 *
 * @tparam T the type of the flag.
 */
template <typename T>
struct FlagParser {};

/// Specialize FlagParser for strings.
template <>
struct FlagParser<std::string> {
  inline static StatusOr<std::string> Parse(char const* v) {
    return std::string(v);
  }
};

/**
 * A class to hold simple command-line values.
 *
 * @tparam T
 */
template <typename T>
class SimpleFlag {
 public:
  SimpleFlag() : is_set_(false) {}

  T const& value() const { return value_.value(); }
  T const& operator*() const { return *value_; }

  /**
   * Parse a positional argument.
   *
   * In the future we may add actual command-line arguments, but all our tests
   * use positional parameters so why bother?
   */
  Status ParsePositional(int& argc, char* argv[], char const* name) {
    // Arguments can be parsed only once.
    if (is_set_.exchange(true)) {
      return Status(StatusCode::kFailedPrecondition,
                    std::string("Positional flag already set: ") + name);
    }
    if (argc < 2) {
      return Status(
          StatusCode::kFailedPrecondition,
          std::string(
              "Not enough command-line arguments for positional flag: ") +
              name);
    }
    auto value = FlagParser<T>::Parse(argv[1]);
    if (!value) {
      return value.status();
    }
    value_ = *std::move(value);
    std::copy(argv + 2, argv + argc, argv + 1);
    argc--;
    return Status();
  }

 private:
  std::atomic<bool> is_set_;
  optional<T> value_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SIMPLE_FLAGS_H_
