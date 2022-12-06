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

#include "google/cloud/internal/make_status.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Status CancelledError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kCancelled, std::move(msg), std::move(info));
}

Status UnknownError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kUnknown, std::move(msg), std::move(info));
}

Status InvalidArgumentError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kInvalidArgument, std::move(msg), std::move(info));
}

Status DeadlineExceededError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kDeadlineExceeded, std::move(msg), std::move(info));
}

Status NotFoundError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kNotFound, std::move(msg), std::move(info));
}

Status AlreadyExistsError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kAlreadyExists, std::move(msg), std::move(info));
}

Status PermissionDeniedError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kPermissionDenied, std::move(msg), std::move(info));
}

Status UnauthenticatedError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kUnauthenticated, std::move(msg), std::move(info));
}

Status ResourceExhaustedError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kResourceExhausted, std::move(msg),
                std::move(info));
}

Status FailedPreconditionError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kFailedPrecondition, std::move(msg),
                std::move(info));
}

Status AbortedError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kAborted, std::move(msg), std::move(info));
}

Status OutOfRangeError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kOutOfRange, std::move(msg), std::move(info));
}

Status UnimplementedError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kUnimplemented, std::move(msg), std::move(info));
}

Status InternalError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kInternal, std::move(msg), std::move(info));
}

Status UnavailableError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kUnavailable, std::move(msg), std::move(info));
}

Status DataLossError(std::string msg, ErrorInfo info) {
  return Status(StatusCode::kDataLoss, std::move(msg), std::move(info));
}

ErrorInfoBuilder::ErrorInfoBuilder(std::string file, int line,
                                   std::string function) {
  metadata_.emplace("gcloud-cpp.version", version_string());
  metadata_.emplace("gcloud-cpp.source.filename", std::move(file));
  metadata_.emplace("gcloud-cpp.source.line", std::to_string(line));
  metadata_.emplace("gcloud-cpp.source.function", std::move(function));
}

ErrorInfo ErrorInfoBuilder::Build(StatusCode code) && {
  return ErrorInfo(reason_.value_or(StatusCodeToString(code)), "gcloud-cpp",
                   std::move(metadata_));
}

Status CancelledError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kCancelled, std::move(msg),
                std::move(b).Build(StatusCode::kCancelled));
}

Status UnknownError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kUnknown, std::move(msg),
                std::move(b).Build(StatusCode::kUnknown));
}

Status InvalidArgumentError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kInvalidArgument, std::move(msg),
                std::move(b).Build(StatusCode::kInvalidArgument));
}

Status DeadlineExceededError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kDeadlineExceeded, std::move(msg),
                std::move(b).Build(StatusCode::kDeadlineExceeded));
}

Status NotFoundError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kNotFound, std::move(msg),
                std::move(b).Build(StatusCode::kNotFound));
}

Status AlreadyExistsError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kAlreadyExists, std::move(msg),
                std::move(b).Build(StatusCode::kAlreadyExists));
}

Status PermissionDeniedError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kPermissionDenied, std::move(msg),
                std::move(b).Build(StatusCode::kPermissionDenied));
}

Status UnauthenticatedError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kUnauthenticated, std::move(msg),
                std::move(b).Build(StatusCode::kUnauthenticated));
}

Status ResourceExhaustedError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kResourceExhausted, std::move(msg),
                std::move(b).Build(StatusCode::kResourceExhausted));
}

Status FailedPreconditionError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kFailedPrecondition, std::move(msg),
                std::move(b).Build(StatusCode::kFailedPrecondition));
}

Status AbortedError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kAborted, std::move(msg),
                std::move(b).Build(StatusCode::kAborted));
}

Status OutOfRangeError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kOutOfRange, std::move(msg),
                std::move(b).Build(StatusCode::kOutOfRange));
}

Status UnimplementedError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kUnimplemented, std::move(msg),
                std::move(b).Build(StatusCode::kUnimplemented));
}

Status InternalError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kInternal, std::move(msg),
                std::move(b).Build(StatusCode::kInternal));
}

Status UnavailableError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kUnavailable, std::move(msg),
                std::move(b).Build(StatusCode::kUnavailable));
}

Status DataLossError(std::string msg, ErrorInfoBuilder b) {
  return Status(StatusCode::kDataLoss, std::move(msg),
                std::move(b).Build(StatusCode::kDataLoss));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
