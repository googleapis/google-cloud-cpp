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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_STATUS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_STATUS_H_

#include "google/cloud/storage/version.h"
#include <iostream>
#include <tuple>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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

  bool ok() const { return 200 == status_code(); }

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
  return os << rhs.error_message() << " [" << rhs.status_code()
            << "], details=" << rhs.error_details();
}

/**
 * Report checksum mismatches as exceptions.
 */
class HashMismatchError : public std::ios_base::failure {
 public:
  explicit HashMismatchError(std::string const& msg, std::string received,
                             std::string computed)
      : std::ios_base::failure(msg),
        received_hash_(std::move(received)),
        computed_hash_(std::move(computed)) {}

  std::string const& received_hash() const { return received_hash_; }
  std::string const& computed_hash() const { return computed_hash_; }

 private:
  std::string received_hash_;
  std::string computed_hash_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_STATUS_H_
