// Copyright 2018 Google LLC
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

#include "google/cloud/status.h"
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
std::string StatusWhat(Status const& status) {
  std::ostringstream os;
  os << status;
  return std::move(os).str();
}
}  // namespace

namespace internal {

// Asserts that Abseil's status codes match ours because we cast between them.
static_assert(static_cast<int>(StatusCode::kOk) ==
                  static_cast<int>(absl::StatusCode::kOk),
              "");
static_assert(static_cast<int>(StatusCode::kCancelled) ==
                  static_cast<int>(absl::StatusCode::kCancelled),
              "");
static_assert(static_cast<int>(StatusCode::kUnknown) ==
                  static_cast<int>(absl::StatusCode::kUnknown),
              "");
static_assert(static_cast<int>(StatusCode::kInvalidArgument) ==
                  static_cast<int>(absl::StatusCode::kInvalidArgument),
              "");
static_assert(static_cast<int>(StatusCode::kDeadlineExceeded) ==
                  static_cast<int>(absl::StatusCode::kDeadlineExceeded),
              "");
static_assert(static_cast<int>(StatusCode::kNotFound) ==
                  static_cast<int>(absl::StatusCode::kNotFound),
              "");
static_assert(static_cast<int>(StatusCode::kAlreadyExists) ==
                  static_cast<int>(absl::StatusCode::kAlreadyExists),
              "");
static_assert(static_cast<int>(StatusCode::kPermissionDenied) ==
                  static_cast<int>(absl::StatusCode::kPermissionDenied),
              "");
static_assert(static_cast<int>(StatusCode::kUnauthenticated) ==
                  static_cast<int>(absl::StatusCode::kUnauthenticated),
              "");
static_assert(static_cast<int>(StatusCode::kResourceExhausted) ==
                  static_cast<int>(absl::StatusCode::kResourceExhausted),
              "");
static_assert(static_cast<int>(StatusCode::kFailedPrecondition) ==
                  static_cast<int>(absl::StatusCode::kFailedPrecondition),
              "");
static_assert(static_cast<int>(StatusCode::kAborted) ==
                  static_cast<int>(absl::StatusCode::kAborted),
              "");
static_assert(static_cast<int>(StatusCode::kOutOfRange) ==
                  static_cast<int>(absl::StatusCode::kOutOfRange),
              "");
static_assert(static_cast<int>(StatusCode::kUnimplemented) ==
                  static_cast<int>(absl::StatusCode::kUnimplemented),
              "");
static_assert(static_cast<int>(StatusCode::kInternal) ==
                  static_cast<int>(absl::StatusCode::kInternal),
              "");
static_assert(static_cast<int>(StatusCode::kUnavailable) ==
                  static_cast<int>(absl::StatusCode::kUnavailable),
              "");
static_assert(static_cast<int>(StatusCode::kDataLoss) ==
                  static_cast<int>(absl::StatusCode::kDataLoss),
              "");

Status MakeStatus(absl::Status status) { return Status(std::move(status)); }
absl::Status ToAbslStatus(Status status) { return std::move(status.status_); }

}  // namespace internal

std::string StatusCodeToString(StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kCancelled:
      return "CANCELLED";
    case StatusCode::kUnknown:
      return "UNKNOWN";
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::kDeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case StatusCode::kNotFound:
      return "NOT_FOUND";
    case StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case StatusCode::kPermissionDenied:
      return "PERMISSION_DENIED";
    case StatusCode::kUnauthenticated:
      return "UNAUTHENTICATED";
    case StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::kAborted:
      return "ABORTED";
    case StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::kUnimplemented:
      return "UNIMPLEMENTED";
    case StatusCode::kInternal:
      return "INTERNAL";
    case StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case StatusCode::kDataLoss:
      return "DATA_LOSS";
    default:
      return "UNEXPECTED_STATUS_CODE=" + std::to_string(static_cast<int>(code));
  }
}

std::ostream& operator<<(std::ostream& os, StatusCode code) {
  return os << StatusCodeToString(code);
}

RuntimeStatusError::RuntimeStatusError(Status status)
    : std::runtime_error(StatusWhat(status)), status_(std::move(status)) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
