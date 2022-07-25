// Copyright 2019 Google LLC
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

#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/status_payload_keys.h"
#include "absl/time/time.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/rpc/error_details.pb.h>
#include <atomic>
#include <sstream>

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

template <typename T>
std::string DebugString(google::protobuf::Any const& any,
                        TracingOptions const& options) {
  T details;
  if (!any.UnpackTo(&details)) return {};
  return DebugString(details, options);
}

std::string DebugString(Status const& status, TracingOptions const& options) {
  std::ostringstream os;
  os << status;
  auto payload =
      internal::GetPayload(status, internal::kStatusPayloadGrpcProto);
  google::rpc::Status proto;
  if (payload && proto.ParseFromString(*payload)) {
    // See https://cloud.google.com/apis/design/errors#error_payloads
    for (google::protobuf::Any const& any : proto.details()) {
      std::string details;
      switch (status.code()) {
        case StatusCode::kInvalidArgument:
          details = DebugString<google::rpc::BadRequest>(any, options);
          break;
        case StatusCode::kFailedPrecondition:
          details = DebugString<google::rpc::PreconditionFailure>(any, options);
          break;
        case StatusCode::kOutOfRange:
          details = DebugString<google::rpc::BadRequest>(any, options);
          break;
        case StatusCode::kNotFound:
        case StatusCode::kAlreadyExists:
          details = DebugString<google::rpc::ResourceInfo>(any, options);
          break;
        case StatusCode::kResourceExhausted:
          details = DebugString<google::rpc::QuotaFailure>(any, options);
          break;
        case StatusCode::kDataLoss:
        case StatusCode::kUnknown:
        case StatusCode::kInternal:
        case StatusCode::kUnavailable:
        case StatusCode::kDeadlineExceeded:
          details = DebugString<google::rpc::DebugInfo>(any, options);
          break;
        case StatusCode::kUnauthenticated:  // NOLINT(bugprone-branch-clone)
        case StatusCode::kPermissionDenied:
        case StatusCode::kAborted:
          // `Status` supports `google.rpc.ErrorInfo` directly.
          break;
        default:
          // Unexpected error details for the status code.
          break;
      }
      if (!details.empty()) {
        os << " + " << details;
        break;
      }
    }
  }
  return std::move(os).str();
}

std::string DebugString(std::string s, TracingOptions const& options) {
  auto const pos =
      static_cast<std::size_t>(options.truncate_string_field_longer_than());
  if (s.size() > pos) s.replace(pos, std::string::npos, "...<truncated>...");
  return s;
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

std::string RequestIdForLogging() {
  static std::atomic<std::uint64_t> generator{0};
  return std::to_string(++generator);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
