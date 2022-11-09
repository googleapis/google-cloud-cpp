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

#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/status_payload_keys.h"
#include "absl/types/optional.h"
#include <google/protobuf/any.pb.h>
#include <google/protobuf/text_format.h>
#include <google/rpc/error_details.pb.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

google::cloud::StatusCode MapStatusCode(grpc::StatusCode const& code) {
  switch (code) {
    case grpc::StatusCode::OK:
      return google::cloud::StatusCode::kOk;
    case grpc::StatusCode::CANCELLED:
      return google::cloud::StatusCode::kCancelled;
    case grpc::StatusCode::UNKNOWN:
      return google::cloud::StatusCode::kUnknown;
    case grpc::StatusCode::INVALID_ARGUMENT:
      return google::cloud::StatusCode::kInvalidArgument;
    case grpc::StatusCode::DEADLINE_EXCEEDED:
      return google::cloud::StatusCode::kDeadlineExceeded;
    case grpc::StatusCode::NOT_FOUND:
      return google::cloud::StatusCode::kNotFound;
    case grpc::StatusCode::ALREADY_EXISTS:
      return google::cloud::StatusCode::kAlreadyExists;
    case grpc::StatusCode::PERMISSION_DENIED:
      return google::cloud::StatusCode::kPermissionDenied;
    case grpc::StatusCode::UNAUTHENTICATED:
      return google::cloud::StatusCode::kUnauthenticated;
    case grpc::StatusCode::RESOURCE_EXHAUSTED:
      return google::cloud::StatusCode::kResourceExhausted;
    case grpc::StatusCode::FAILED_PRECONDITION:
      return google::cloud::StatusCode::kFailedPrecondition;
    case grpc::StatusCode::ABORTED:
      return google::cloud::StatusCode::kAborted;
    case grpc::StatusCode::OUT_OF_RANGE:
      return google::cloud::StatusCode::kOutOfRange;
    case grpc::StatusCode::UNIMPLEMENTED:
      return google::cloud::StatusCode::kUnimplemented;
    case grpc::StatusCode::INTERNAL:
      return google::cloud::StatusCode::kInternal;
    case grpc::StatusCode::UNAVAILABLE:
      return google::cloud::StatusCode::kUnavailable;
    case grpc::StatusCode::DATA_LOSS:
      return google::cloud::StatusCode::kDataLoss;
    default:
      return google::cloud::StatusCode::kUnknown;
  }
}

// Unpacks the ErrorInfo from the Status proto, if one exists.
absl::optional<google::rpc::ErrorInfo> GetErrorInfoProto(
    google::rpc::Status const& proto) {
  // While in theory there _could_ be multiple ErrorInfo protos in this
  // repeated field, we're told that there will be at most one, and our
  // user-facing APIs should only expose one. So if we find one, we're done.
  google::rpc::ErrorInfo error_info;
  for (google::protobuf::Any const& any : proto.details()) {
    if (any.UnpackTo(&error_info)) return error_info;
  }
  return absl::nullopt;
}

ErrorInfo GetErrorInfo(google::rpc::Status const& status) {
  auto proto = GetErrorInfoProto(status);
  if (!proto) return {};
  std::unordered_map<std::string, std::string> metadata;
  for (auto const& e : proto->metadata()) metadata[e.first] = e.second;
  return ErrorInfo{proto->reason(), proto->domain(), std::move(metadata)};
}

}  // namespace

google::cloud::Status MakeStatusFromRpcError(grpc::Status const& status) {
  // Fast path for "OK" statuses, which cannot have messages or payloads.
  if (status.ok()) return Status{};
  auto const e = status.error_details();
  if (!e.empty()) {
    google::rpc::Status proto;
    if (!proto.ParseFromString(e)) {
      return MakeStatusFromRpcError(
          status.error_code(),
          status.error_message() + " (discarded invalid error_details)");
    }
    return MakeStatusFromRpcError(proto);
  }
  return MakeStatusFromRpcError(status.error_code(), status.error_message());
}

google::cloud::Status MakeStatusFromRpcError(grpc::StatusCode code,
                                             std::string what) {
  return google::cloud::Status(MapStatusCode(code), std::move(what));
}

google::cloud::Status MakeStatusFromRpcError(google::rpc::Status const& proto) {
  // Fast path for "OK" statuses, which cannot have messages or payloads.
  if (proto.code() == static_cast<std::int32_t>(StatusCode::kOk))
    return Status{};
  StatusCode code = StatusCode::kUnknown;
  if (proto.code() >= 0 &&
      proto.code() <= static_cast<std::int32_t>(StatusCode::kUnauthenticated)) {
    code = static_cast<StatusCode>(proto.code());
  }
  auto status = Status(code, proto.message(), GetErrorInfo(proto));
  google::cloud::internal::SetPayload(
      status, google::cloud::internal::StatusPayloadGrpcProto(),
      proto.SerializeAsString());
  return status;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
