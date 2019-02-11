// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/assert_ok.h"

namespace testing {
namespace internal {

namespace {

std::string GrpcErrorCodeName(::grpc::StatusCode code) {
  return ::google::cloud::StatusCodeToString(
      static_cast<::google::cloud::StatusCode>(code));
}

}  // namespace

// A unary predicate-formatter for google::cloud::Status.
testing::AssertionResult IsOkPredFormat(char const* expr,
                                        ::google::cloud::Status const& status) {
  if (status.ok()) {
    return testing::AssertionSuccess();
  }
  return testing::AssertionFailure()
         << "Status of \"" << expr
         << "\" is expected to be OK, but evaluates to \"" << status.message()
         << "\" (code " << status.code() << ")";
}

// A unary predicate-formatter for grpc::Status.
testing::AssertionResult IsOkPredFormat(char const* expr,
                                        ::grpc::Status const& status) {
  if (status.ok()) {
    return testing::AssertionSuccess();
  }
  return testing::AssertionFailure()
         << "Status of \"" << expr
         << "\" is expected to be OK, but evaluates to \""
         << status.error_message() << "\" (code "
         << GrpcErrorCodeName(status.error_code()) << ")";
}

}  // namespace internal
}  // namespace testing
