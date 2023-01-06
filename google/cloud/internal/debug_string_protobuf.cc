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

#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "absl/time/time.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/timestamp.pb.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

class DurationMessagePrinter
    : public google::protobuf::TextFormat::MessagePrinter {
 public:
  ~DurationMessagePrinter() override = default;
  void Print(google::protobuf::Message const& message, bool single_line_mode,
             google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    auto const* reflection = message.GetReflection();
    auto const* descriptor = message.GetDescriptor();
    auto seconds = reflection->GetInt64(message, descriptor->field(0));
    auto nanos = reflection->GetInt32(message, descriptor->field(1));
    auto d = absl::Seconds(seconds) + absl::Nanoseconds(nanos);
    generator->PrintLiteral("\"");
    generator->PrintString(absl::FormatDuration(d));
    generator->PrintLiteral("\"");
    generator->Print(single_line_mode ? " " : "\n", 1);
  }
};

class TimestampMessagePrinter
    : public google::protobuf::TextFormat::MessagePrinter {
 public:
  ~TimestampMessagePrinter() override = default;
  void Print(google::protobuf::Message const& message, bool single_line_mode,
             google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    auto const* reflection = message.GetReflection();
    auto const* descriptor = message.GetDescriptor();
    auto seconds = reflection->GetInt64(message, descriptor->field(0));
    auto nanos = reflection->GetInt32(message, descriptor->field(1));
    auto t = absl::FromUnixSeconds(seconds) + absl::Nanoseconds(nanos);
    auto constexpr kFormat = "\"%E4Y-%m-%dT%H:%M:%E*SZ\"";
    generator->PrintString(absl::FormatTime(kFormat, t, absl::UTCTimeZone()));
    generator->Print(single_line_mode ? " " : "\n", 1);
  }
};

}  // namespace

std::string DebugString(google::protobuf::Message const& m,
                        TracingOptions const& options) {
  std::string str;
  google::protobuf::TextFormat::Printer p;
  p.SetSingleLineMode(options.single_line_mode());
  if (!options.single_line_mode()) p.SetInitialIndentLevel(1);
  p.SetUseShortRepeatedPrimitives(options.use_short_repeated_primitives());
  p.SetTruncateStringFieldLongerThan(
      options.truncate_string_field_longer_than());
  p.SetPrintMessageFieldsInIndexOrder(true);
  p.SetExpandAny(true);
  p.RegisterMessagePrinter(google::protobuf::Duration::descriptor(),
                           new DurationMessagePrinter);
  p.RegisterMessagePrinter(google::protobuf::Timestamp::descriptor(),
                           new TimestampMessagePrinter);
  p.PrintToString(m, &str);
  return absl::StrCat(m.GetTypeName(), " {",
                      (options.single_line_mode() ? " " : "\n"), str, "}");
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
