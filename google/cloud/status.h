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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H_

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
enum StatusCode {
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
  kDoNotUse = -1
};

std::string StatusCodeToString(StatusCode code);
std::ostream& operator<<(std::ostream& os, StatusCode code);

/**
 * Reports error code and details from a remote request.
 *
 * This class is modeled after `grpc::Status`, it contains the status code and
 * error message (if applicable) from a JSON request.
 *
 * TODO(#548) - define well-known error codes.
 */
class Status {
 public:
  Status() : status_code_(200) {}

  explicit Status(long status_code, std::string error_message)
      : status_code_(status_code),
        error_message_(std::move(error_message)),
        error_details_() {}

  explicit Status(long status_code, std::string error_message,
                  std::string error_details)
      : status_code_(status_code),
        error_message_(std::move(error_message)),
        error_details_(std::move(error_details)) {}

  bool ok() const { return 200 == status_code() or 0 == status_code(); }

  bool operator==(Status const& rhs) const {
    return status_code() == rhs.status_code() and
           error_message() == rhs.error_message() and
           error_details() == rhs.error_details();
  }
  bool operator!=(Status const& rhs) const { return not(*this == rhs); }
  bool operator<(Status const& rhs) const {
    return std::tie(status_code_, error_message_, error_details_) <
           std::tie(rhs.status_code_, rhs.error_message_, rhs.error_details_);
  }
  bool operator>=(Status const& rhs) const { return not(*this < rhs); }
  bool operator>(Status const& rhs) const { return rhs < *this; }
  bool operator<=(Status const& rhs) const { return rhs >= *this; }

  long status_code() const { return status_code_; }
  std::string const& error_message() const { return error_message_; }
  std::string const& error_details() const { return error_details_; }

 private:
  long status_code_;
  std::string error_message_;
  std::string error_details_;
};

inline std::ostream& operator<<(std::ostream& os, Status const& rhs) {
  return os << rhs.error_message()
            << StatusCodeToString(static_cast<StatusCode>(rhs.status_code()))
            << " [" << rhs.status_code()
            << "], details=" << rhs.error_details();
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H_
