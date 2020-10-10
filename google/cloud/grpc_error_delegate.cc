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

#include "google/cloud/grpc_error_delegate.h"
#include <google/protobuf/text_format.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
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
}  // namespace

google::cloud::Status MakeStatusFromRpcError(grpc::Status const& status) {
  return MakeStatusFromRpcError(status.error_code(), status.error_message());
}

google::cloud::Status MakeStatusFromRpcError(grpc::StatusCode code,
                                             std::string what) {
  // TODO(#1912): Pass along status.error_details() once we have `absl::Status`
  // or some version that supports binary blobs of data.
  return google::cloud::Status(MapStatusCode(code), std::move(what));
}

google::cloud::Status MakeStatusFromRpcError(
    google::rpc::Status const& status) {
  StatusCode code = StatusCode::kUnknown;
  if (status.code() >= 0 &&
      status.code() <=
          static_cast<std::int32_t>(StatusCode::kUnauthenticated)) {
    code = static_cast<StatusCode>(status.code());
  }
  return Status(code, status.message());
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
