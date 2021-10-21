// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTRACT_LONG_RUNNING_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTRACT_LONG_RUNNING_RESULT_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/any.pb.h>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// Extracts the value (or error) from a completed long-running operation
Status ExtractOperationResultMetadataImpl(
    StatusOr<google::longrunning::Operation> op,
    google::protobuf::Message& result,
    absl::FunctionRef<bool(google::protobuf::Any const&)> validate_any,
    std::string const& location);

/// Extracts the value (or error) from a completed long-running operation
Status ExtractOperationResultResponseImpl(
    StatusOr<google::longrunning::Operation> op,
    google::protobuf::Message& result,
    absl::FunctionRef<bool(google::protobuf::Any const&)> validate_any,
    std::string const& location);

/**
 * Extracts the value from a completed long-running operation.
 *
 * This helper is used in `AsyncLongRunningOperation()` to extract the value (or
 * error) from a completed long-running operation.
 */
template <typename ReturnType>
StatusOr<ReturnType> ExtractLongRunningResultMetadata(
    StatusOr<google::longrunning::Operation> op, std::string const& location) {
  ReturnType result;
  auto status = ExtractOperationResultMetadataImpl(
      std::move(op), result,
      [](google::protobuf::Any const& any) { return any.Is<ReturnType>(); },
      location);
  if (!status.ok()) return status;
  return result;
}

/**
 * Extracts the value from a completed long-running operation.
 *
 * This helper is used in `AsyncLongRunningOperation()` to extract the value (or
 * error) from a completed long-running operation.
 */
template <typename ReturnType>
StatusOr<ReturnType> ExtractLongRunningResultResponse(
    StatusOr<google::longrunning::Operation> op, std::string const& location) {
  ReturnType result;
  auto status = ExtractOperationResultResponseImpl(
      std::move(op), result,
      [](google::protobuf::Any const& any) { return any.Is<ReturnType>(); },
      location);
  if (!status.ok()) return status;
  return result;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTRACT_LONG_RUNNING_RESULT_H
