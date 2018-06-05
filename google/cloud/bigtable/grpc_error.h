// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_GRPC_ERROR_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_GRPC_ERROR_H_

#include "google/cloud/bigtable/version.h"
#include <grpcpp/grpcpp.h>
#include <stdexcept>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Wrap gRPC errors is a C++ exception.
 *
 * Applications that only wish to log errors can (and should) catch
 * `std::exception` or `std::runtime_error`. The string returned by `what()`
 * contains all the necessary information.  If the application wants to handle
 * errors raised by the gRPC library, they can catch this exception type, and
 * use the `error_code()` member function to implement whatever error handling
 * strategy they need.
 *
 * It is customary, though not required, for C++ libraries and applications
 * to use classes in the `std::exception` hierarchy to report errors. Meanwhile,
 * gRPC reports errors using the `grpc::Status` class. This class wraps the
 * contents of a `grpc::Status` in an exception class.  If the application is
 * interested in the details of the exception it can catch it and examine all
 * fields.  If the application developers simply want to log all exceptions they
 * can catch `std::exception` and log the contents of `what()`.
 */
class GRpcError : public std::runtime_error {
 public:
  explicit GRpcError(char const* what, grpc::Status const& status);

  //@{
  /// @name accessors.
  grpc::StatusCode error_code() const { return error_code_; }
  std::string const& error_message() const { return error_message_; }
  std::string const& error_details() const { return error_details_; }
  //@}

 private:
  /// Return the what() string given @p status.
  static std::string CreateWhatString(char const* what,
                                      grpc::Status const& status);

 private:
  grpc::StatusCode error_code_;
  std::string error_message_;
  std::string error_details_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_GRPC_ERROR_H_
