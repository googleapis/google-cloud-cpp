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

#include "google/cloud/internal/debug_string_status.h"
#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/internal/status_payload_keys.h"
#include <google/protobuf/any.pb.h>
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

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
      internal::GetPayload(status, internal::StatusPayloadGrpcProto());
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

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
