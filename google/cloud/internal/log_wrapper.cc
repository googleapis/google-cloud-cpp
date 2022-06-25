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
#include <google/protobuf/text_format.h>
#include <google/rpc/error_details.pb.h>
#include <atomic>
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string DebugString(google::protobuf::Message const& m,
                        TracingOptions const& options) {
  std::string str;
  google::protobuf::TextFormat::Printer p;
  p.SetSingleLineMode(options.single_line_mode());
  if (!options.single_line_mode()) p.SetInitialIndentLevel(1);
  p.SetUseShortRepeatedPrimitives(options.use_short_repeated_primitives());
  p.SetTruncateStringFieldLongerThan(
      options.truncate_string_field_longer_than());
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
  std::size_t const pos = options.truncate_string_field_longer_than();
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
