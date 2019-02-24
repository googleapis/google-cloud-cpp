// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/terminate_handler.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace {
StatusCode MapStatusCode(grpc::StatusCode const& code) {
  switch (code) {
    case grpc::StatusCode::OK:
      return StatusCode::kOk;
    case grpc::StatusCode::CANCELLED:
      return StatusCode::kCancelled;
    case grpc::StatusCode::UNKNOWN:
      return StatusCode::kUnknown;
    case grpc::StatusCode::INVALID_ARGUMENT:
      return StatusCode::kInvalidArgument;
    case grpc::StatusCode::DEADLINE_EXCEEDED:
      return StatusCode::kDeadlineExceeded;
    case grpc::StatusCode::NOT_FOUND:
      return StatusCode::kNotFound;
    case grpc::StatusCode::ALREADY_EXISTS:
      return StatusCode::kAlreadyExists;
    case grpc::StatusCode::PERMISSION_DENIED:
      return StatusCode::kPermissionDenied;
    case grpc::StatusCode::UNAUTHENTICATED:
      return StatusCode::kUnauthenticated;
    case grpc::StatusCode::RESOURCE_EXHAUSTED:
      return StatusCode::kResourceExhausted;
    case grpc::StatusCode::FAILED_PRECONDITION:
      return StatusCode::kFailedPrecondition;
    case grpc::StatusCode::ABORTED:
      return StatusCode::kAborted;
    case grpc::StatusCode::OUT_OF_RANGE:
      return StatusCode::kOutOfRange;
    case grpc::StatusCode::UNIMPLEMENTED:
      return StatusCode::kUnimplemented;
    case grpc::StatusCode::INTERNAL:
      return StatusCode::kInternal;
    case grpc::StatusCode::UNAVAILABLE:
      return StatusCode::kUnavailable;
    case grpc::StatusCode::DATA_LOSS:
      return StatusCode::kDataLoss;
    default:
      return StatusCode::kUnknown;
  }
}
}  // namespace

google::cloud::Status MakeStatusFromRpcError(grpc::Status const& status) {
  StatusCode code = MapStatusCode(status.error_code());
  // TODO(#1912): Pass along status.error_details() once we have absl::Status
  // or some version that supports binary blobs of data.
  return google::cloud::Status(code, status.error_message());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
