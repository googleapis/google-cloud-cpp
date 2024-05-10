// Copyright 2021 Google LLC
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

#include "google/cloud/internal/extract_long_running_result.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Status ExtractOperationResultMetadataImpl(
    StatusOr<google::longrunning::Operation> op,
    google::protobuf::Message& result,
    absl::FunctionRef<bool(google::protobuf::Any const&)> validate_any,
    std::string const& location) {
  if (!op) return std::move(op).status();
  if (op->has_error()) return MakeStatusFromRpcError(op->error());
  if (!op->has_metadata()) {
    return internal::InternalError(location +
                                       "() cannot extract value "
                                       "from operation without error or "
                                       "metadata, name=" +
                                       op->name(),
                                   GCP_ERROR_INFO());
  }
  google::protobuf::Any const& any = op->metadata();
  if (!validate_any(any)) {
    return internal::InternalError(location +
                                       "() operation completed "
                                       "with an invalid metadata type, name=" +
                                       op->name(),
                                   GCP_ERROR_INFO());
  }
  any.UnpackTo(&result);
  return Status{};
}

Status ExtractOperationResultResponseImpl(
    StatusOr<google::longrunning::Operation> op,
    google::protobuf::Message& result,
    absl::FunctionRef<bool(google::protobuf::Any const&)> validate_any,
    std::string const& location) {
  if (!op) return std::move(op).status();
  if (op->has_error()) return MakeStatusFromRpcError(op->error());
  if (!op->has_response()) {
    return internal::InternalError(location +
                                       "() cannot extract value "
                                       "from operation without error or "
                                       "response, name=" +
                                       op->name(),
                                   GCP_ERROR_INFO());
  }
  google::protobuf::Any const& any = op->response();
  if (!validate_any(any)) {
    return internal::InternalError(location +
                                       "() operation completed "
                                       "with an invalid response type, name=" +
                                       op->name(),
                                   GCP_ERROR_INFO());
  }
  any.UnpackTo(&result);
  return Status{};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
