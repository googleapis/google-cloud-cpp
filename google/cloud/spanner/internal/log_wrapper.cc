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

#include "google/cloud/spanner/internal/log_wrapper.h"
#include <google/protobuf/text_format.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
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

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
