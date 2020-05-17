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
#include <iostream>
#include <tuple>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Well-known status codes with `grpc::StatusCode`-compatible values.
 *
 * The semantics of these values are documented in:
 *     https://grpc.io/grpc/cpp/classgrpc_1_1_status.html
 *
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

/**
 * Reports error code and details from a remote request.
 *
 * This class is modeled after `grpc::Status`, it contains the status code and
 * error message (if applicable) from a JSON request.
 */
class Status {
 public:
  Status() = default;

  explicit Status(StatusCode status_code, std::string message)
      : code_(status_code), message_(std::move(message)) {}

  bool ok() const { return code_ == StatusCode::kOk; }

  StatusCode code() const { return code_; }
  std::string const& message() const { return message_; }

 private:
  StatusCode code_{StatusCode::kOk};
  std::string message_;
};

inline std::ostream& operator<<(std::ostream& os, Status const& rhs) {
  return os << rhs.message() << " [" << StatusCodeToString(rhs.code()) << "]";
}

inline bool operator==(Status const& lhs, Status const& rhs) {
  return lhs.code() == rhs.code() && lhs.message() == rhs.message();
}

inline bool operator!=(Status const& lhs, Status const& rhs) {
  return !(lhs == rhs);
}

class RuntimeStatusError : public std::runtime_error {
 public:
  explicit RuntimeStatusError(Status status);

  Status const& status() const { return status_; }

 private:
  Status status_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
