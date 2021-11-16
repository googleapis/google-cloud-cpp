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
#include "absl/types/optional.h"
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Well-known status codes with `grpc::StatusCode`-compatible values.
 *
 * The semantics of these values are documented in:
 *     https://grpc.io/grpc/cpp/classgrpc_1_1_status.html
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
// Gets/sets payload data for non-OK Statuses.
void SetPayload(Status&, std::string key, std::string payload);
absl::optional<std::string> GetPayload(Status&, std::string const& key);
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
      : code_(status_code), message_(std::move(message)) {}

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }
  std::string const& message() const { return message_; }

  friend inline bool operator==(Status const& a, Status const& b) {
    return a.code_ == b.code_ && a.message_ == b.message_ &&
           a.payload_ == b.payload_;
  }
  friend inline bool operator!=(Status const& a, Status const& b) {
    return !(a == b);
  }
  friend inline std::ostream& operator<<(std::ostream& os, Status const& s) {
    return os << s.message() << " [" << s.code() << "]";
  }

 private:
  friend void internal::SetPayload(Status&, std::string, std::string);
  friend absl::optional<std::string> internal::GetPayload(Status&,
                                                          std::string const&);

  StatusCode code_{StatusCode::kOk};
  std::string message_;
  std::unordered_map<std::string, std::string> payload_;
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
