// Copyright 2019 Google LLC
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

#include "google/cloud/internal/log_wrapper.h"
#include <google/protobuf/text_format.h>
#include <atomic>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::string DebugString(google::protobuf::Message const& m,
                        TracingOptions const& options) {
  std::string str;
  google::protobuf::TextFormat::Printer p;
  p.SetSingleLineMode(options.single_line_mode());
  p.SetUseShortRepeatedPrimitives(options.use_short_repeated_primitives());
  p.SetTruncateStringFieldLongerThan(
      options.truncate_string_field_longer_than());
  p.PrintToString(m, &str);
  return str;
}

std::string RequestIdForLogging() {
  static std::atomic<std::uint64_t> generator{0};
  return std::to_string(++generator);
}

char const* DebugFutureStatus(std::future_status status) {
  // We cannot log the value of the future, even when it is available, because
  // the value can only be extracted once. But we can log if the future is
  // satisfied.
  char const* msg = "<invalid>";
  switch (status) {
    case std::future_status::ready:
      msg = "ready";
      break;
    case std::future_status::timeout:
      msg = "timeout";
      break;
    case std::future_status::deferred:
      msg = "deferred";
      break;
  }
  return msg;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
