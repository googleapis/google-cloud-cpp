// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TRACING_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TRACING_OPTIONS_H

#include "google/cloud/version.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * The configuration parameters for RPC/protobuf tracing.
 *
 * The default options are:
 *   single_line_mode=on
 *   use_short_repeated_primitives=on
 *   truncate_string_field_longer_than=128
 */
class TracingOptions {
 public:
  TracingOptions() = default;
  TracingOptions(TracingOptions const&) = default;
  TracingOptions& operator=(TracingOptions const&) = default;
  TracingOptions(TracingOptions&&) = default;
  TracingOptions& operator=(TracingOptions&&) = default;

  friend bool operator==(TracingOptions const& a, TracingOptions const& b);
  friend bool operator!=(TracingOptions const& a, TracingOptions const& b) {
    return !(a == b);
  }

  /// Override the default options with values from @p `str`.
  TracingOptions& SetOptions(std::string const& str);

  /// The entire message will be output on a single line with no line breaks.
  bool single_line_mode() const { return single_line_mode_; }

  /// Print repeated primitives in a compact format instead of each value on
  /// its own line.
  bool use_short_repeated_primitives() const {
    return use_short_repeated_primitives_;
  }

  /// If non-zero, truncate all string/bytes fields longer than this.
  std::int64_t truncate_string_field_longer_than() const {
    return truncate_string_field_longer_than_;
  }

 private:
  bool single_line_mode_ = true;
  bool use_short_repeated_primitives_ = true;
  std::int64_t truncate_string_field_longer_than_ = 128;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TRACING_OPTIONS_H
