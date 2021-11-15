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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H

#include "google/cloud/version.h"
#include "absl/status/status.h"
#include <iostream>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Well-known status codes with values that are compatible with
 * `grpc::StatusCode` and `absl::StatusCode`.
 *
 * @see https://grpc.io/grpc/cpp/classgrpc_1_1_status.html
 * @see https://github.com/abseil/abseil-cpp/blob/master/absl/status/status.h
 */
enum class StatusCode {
  /// Not an error; returned on success.
  kOk = 0,

  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kUnauthenticated = 16,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
};

std::string StatusCodeToString(StatusCode code);
std::ostream& operator<<(std::ostream& os, StatusCode code);

class Status;
namespace internal {
Status MakeStatus(absl::Status);
absl::Status MakeAbslStatus(Status);
}  // namespace internal

/**
 * Represents success or an error with info about the error.

 * This class is typically used to indicate whether or not a function or other
 * operation completed successfully. Success is indicated by an "OK" status
 * (`StatusCode::kOk`), and the `.ok()` method will return true. Any non-OK
 * `Status` is considered an error. Users can inspect the error using the
 * `.code()` and `.message()` member functions, or they can simply stream the
 * `Status` object, and it will print itself in some human readable way (the
 * streamed format may change over time and you should *not* depend on the
 * specific format of a streamed `Status` object remaining unchanged).
 *
 * This is a regular value type that can be copied, moved, compared for
 * equality, and streamed.
 */
class Status {
 public:
  Status() = default;

  explicit Status(StatusCode status_code, std::string message)
      : status_(static_cast<absl::StatusCode>(status_code), message),
        message_(std::move(message)) {}

  bool ok() const { return status_.ok(); }
  StatusCode code() const { return static_cast<StatusCode>(status_.code()); }
  std::string const& message() const { return message_; }

  friend inline bool operator==(Status const& a, Status const& b) {
    return a.status_ == b.status_;
  }
  friend inline bool operator!=(Status const& a, Status const& b) {
    return !(a == b);
  }
  friend inline std::ostream& operator<<(std::ostream& os, Status const& s) {
    return os << s.message() << " [" << s.code() << "]";
  }

 private:
  friend Status internal::MakeStatus(absl::Status);
  friend absl::Status internal::MakeAbslStatus(Status);

  explicit Status(absl::Status status)
      : status_(std::move(status)), message_(status_.message()) {}

  // The implementation primarily forwards to absl::Status, with the exception
  // that we need to keep a copy of the message string because we need to
  // return a const ref rather than a string_view.
  absl::Status status_;
  std::string message_;
};

class RuntimeStatusError : public std::runtime_error {
 public:
  explicit RuntimeStatusError(Status status);

  Status const& status() const { return status_; }

 private:
  Status status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
