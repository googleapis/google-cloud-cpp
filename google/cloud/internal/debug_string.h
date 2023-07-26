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
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include <chrono>
#include <map>
#include <string>
#include <vector>

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
  DebugFormatter(absl::string_view name, TracingOptions options,
                 int indent = 0);

  template <typename T>
  DebugFormatter& SubMessage(absl::string_view name, T const& message) {
    absl::StrAppend(&str_, message.DebugString(name, options_, indent_));
    return *this;
  }

  // Specialized support for types without their own DebugString().
  DebugFormatter& Field(absl::string_view field_name, bool value);
  DebugFormatter& Field(absl::string_view field_name,
                        std::chrono::system_clock::time_point value);
  DebugFormatter& Field(
      absl::string_view field_name,
      absl::optional<std::chrono::system_clock::time_point> value);
  DebugFormatter& Field(absl::string_view field_name,
                        std::vector<std::string> const& value);
  DebugFormatter& Field(absl::string_view field_name,
                        std::map<std::string, std::string> const& value);
  DebugFormatter& Field(absl::string_view field_name,
                        std::multimap<std::string, std::string> const& value);

  template <typename T>
  DebugFormatter& Field(absl::string_view field_name, T const& value) {
    absl::StrAppend(&str_, Sep(), field_name, ": ", value);
    return *this;
  }

  template <class Rep, class Period = std::ratio<1>>
  DebugFormatter& Field(absl::string_view field_name,
                        std::chrono::duration<Rep, Period> value) {
    absl::StrAppend(&str_, Sep(), field_name, " {");
    ++indent_;
    absl::StrAppend(&str_, Sep(), "\"",
                    absl::FormatDuration(absl::FromChrono(value)), "\"");
    --indent_;
    absl::StrAppend(&str_, Sep(), "}");
    return *this;
  }

  template <typename T>
  DebugFormatter& Field(absl::string_view field_name,
                        absl::optional<T> const& value) {
    if (value) {
      absl::StrAppend(&str_, value->DebugString(field_name, options_, indent_));
    }
    return *this;
  }

  template <typename T>
  DebugFormatter& Field(absl::string_view field_name,
                        std::vector<T> const& value) {
    for (auto const& e : value) {
      absl::StrAppend(&str_, e.DebugString(field_name, options_, indent_));
    }
    return *this;
  }

  template <typename T>
  DebugFormatter& Field(absl::string_view field_name,
                        std::map<std::string, T> const& value) {
    for (auto const& e : value) {
      absl::StrAppend(&str_, Sep(), field_name, " {");
      ++indent_;
      absl::StrAppend(&str_, Sep(), "key: ", "\"", e.first, "\"");
      absl::StrAppend(&str_, e.second.DebugString("value", options_, indent_));
      --indent_;
      absl::StrAppend(&str_, Sep(), "}");
    }
    return *this;
  }

  DebugFormatter& StringField(absl::string_view field_name, std::string value);

  std::string Build();

 private:
  std::string Sep() const;

  TracingOptions options_;
  std::string str_;
  int indent_;
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
