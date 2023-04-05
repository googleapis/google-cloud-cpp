// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DEBUG_STRING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DEBUG_STRING_H

#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Build strings for use in request/response logging.
 *
 * The intent is to produce strings with a format similar to those returned
 * by `DebugString(google::protobuf::Message const&, TracingOptions const&)`
 * from `google/cloud/internal/debug_string_protobuf.h` for a proto message.
 */
class DebugFormatter {
 public:
  DebugFormatter(TracingOptions options, absl::string_view message_name);

  DebugFormatter& SubMessage(absl::string_view message_name);
  DebugFormatter& EndMessage();

  DebugFormatter& Field(absl::string_view field_name, bool value);

  template <typename T>
  DebugFormatter& Field(absl::string_view field_name, T const& value) {
    absl::StrAppend(&str_, Sep(), field_name, ": ", value);
    return *this;
  }

  template <typename T>
  DebugFormatter& QuotedField(absl::string_view field_name, T const& value) {
    absl::StrAppend(&str_, Sep(), field_name, ": ", "\"", value, "\"");
    return *this;
  }

  template <typename T>
  DebugFormatter& QuotedField(T const& value) {
    absl::StrAppend(&str_, Sep(), "\"", value, "\"");
    return *this;
  }

  DebugFormatter& StringField(absl::string_view field_name, std::string value);

  std::string Build();

 private:
  std::string Sep() const;

  TracingOptions options_;
  std::string str_;
  int indent_ = 0;
};

// Return @p s with possible length restriction due to the application of
// `TracingOptions::truncate_string_field_longer_than()`.
std::string DebugString(std::string s, TracingOptions const& options);

// Create a unique ID that can be used to match asynchronous requests/response
// pairs.
std::string RequestIdForLogging();

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DEBUG_STRING_H
