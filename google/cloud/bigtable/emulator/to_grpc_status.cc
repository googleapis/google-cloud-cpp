// Copyright 2024 Google LLC
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

#include "google/cloud/bigtable/emulator/to_grpc_status.h"
#include "google/rpc/status.pb.h"
#include <google/protobuf/any.pb.h>
#include <google/rpc/error_details.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

/// Translate google::cloud::StatusCode into grpc::StatusCode.
grpc::StatusCode MapStatusCode(google::cloud::StatusCode code) {
  switch (code) {
    case google::cloud::StatusCode::kOk:
      return grpc::StatusCode::OK;
    case google::cloud::StatusCode::kCancelled:
      return grpc::StatusCode::CANCELLED;
    case google::cloud::StatusCode::kUnknown:
      return grpc::StatusCode::UNKNOWN;
    case google::cloud::StatusCode::kInvalidArgument:
      return grpc::StatusCode::INVALID_ARGUMENT;
    case google::cloud::StatusCode::kDeadlineExceeded:
      return grpc::StatusCode::DEADLINE_EXCEEDED;
    case google::cloud::StatusCode::kNotFound:
      return grpc::StatusCode::NOT_FOUND;
    case google::cloud::StatusCode::kAlreadyExists:
      return grpc::StatusCode::ALREADY_EXISTS;
    case google::cloud::StatusCode::kPermissionDenied:
      return grpc::StatusCode::PERMISSION_DENIED;
    case google::cloud::StatusCode::kUnauthenticated:
      return grpc::StatusCode::UNAUTHENTICATED;
    case google::cloud::StatusCode::kResourceExhausted:
      return grpc::StatusCode::RESOURCE_EXHAUSTED;
    case google::cloud::StatusCode::kFailedPrecondition:
      return grpc::StatusCode::FAILED_PRECONDITION;
    case google::cloud::StatusCode::kAborted:
      return grpc::StatusCode::ABORTED;
    case google::cloud::StatusCode::kOutOfRange:
      return grpc::StatusCode::OUT_OF_RANGE;
    case google::cloud::StatusCode::kUnimplemented:
      return grpc::StatusCode::UNIMPLEMENTED;
    case google::cloud::StatusCode::kInternal:
      return grpc::StatusCode::INTERNAL;
    case google::cloud::StatusCode::kUnavailable:
      return grpc::StatusCode::UNAVAILABLE;
    case google::cloud::StatusCode::kDataLoss:
      return grpc::StatusCode::DATA_LOSS;
    default:
      return grpc::StatusCode::UNKNOWN;
  }
}

::grpc::Status ToGrpcStatus(Status const& to_convert) {
  google::rpc::ErrorInfo error_info;
  error_info.set_reason(to_convert.error_info().reason());
  error_info.set_domain(to_convert.error_info().domain());
  for (auto const& md_name_value : to_convert.error_info().metadata()) {
    (*error_info.mutable_metadata())[md_name_value.first] =
        md_name_value.second;
  }

  google::rpc::Status rpc_status;
  rpc_status.set_code(static_cast<std::int32_t>(to_convert.code()));
  rpc_status.set_message(to_convert.message());
  auto& rpc_status_details = *rpc_status.add_details();
  rpc_status_details.PackFrom(std::move(error_info));

  std::string serialized_rpc_status;
  rpc_status.SerializeToString(&serialized_rpc_status);
  return ::grpc::Status(MapStatusCode(to_convert.code()), to_convert.message(),
                        std::move(serialized_rpc_status));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
