// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H

#include "google/cloud/internal/rest_context.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/**
 * Returns options used in the `StorageConnectionImpl` tests.
 *
 * These options set the retry policy to accept at most 2 transient errors. The
 * backoff policy uses very short backoffs. This works well in unit tests. The
 * idempotency policy retries all operations.
 */
Options RetryTestOptions();

/// Validates the `Status` produced in a "too many transients" test.
::testing::Matcher<Status> StoppedOnTooManyTransients(char const* api_name);

/// Validates the `Status` produced in a "permanent error" test.
::testing::Matcher<Status> StoppedOnPermanentError(char const* api_name);

/// Validates the idempotency tokens used in a retry loop.
::testing::Matcher<std::vector<std::string>> RetryLoopUsesSingleToken();

/// Validates the Options used in a retry loop.
::testing::Matcher<std::vector<std::string>> RetryLoopUsesOptions();

/// Captures the idempotency token. Refactors some code in
/// MockRetryClientFunction.
void CaptureIdempotencyToken(std::vector<std::string>& tokens,
                             rest_internal::RestContext const& context);

/// Captures the authority values.
void CaptureAuthorityOption(std::vector<std::string>& authority,
                            rest_internal::RestContext const& context);

/// Captures information to validate the StorageConnectionImpl loops and return
/// a transient error.
class MockRetryClientFunction {
 public:
  explicit MockRetryClientFunction(Status status);

  std::vector<std::string> const& captured_tokens() const { return *tokens_; }
  std::vector<std::string> const& captured_authority_options() const {
    return *authority_options_;
  }

  template <typename Request>
  Status operator()(rest_internal::RestContext& context, Options const&,
                    Request const&) {
    CaptureIdempotencyToken(*tokens_, context);
    CaptureAuthorityOption(*authority_options_, context);
    return status_;
  }

 private:
  Status status_;
  // These must be shared between copied instances. We use this class as mock
  // functions for `.WillOnce()` and `.WillRepeatedly()`, both of which make a
  // copy, and then we examine the contents of these (shared) member variables.
  std::shared_ptr<std::vector<std::string>> tokens_ =
      std::make_shared<std::vector<std::string>>();
  std::shared_ptr<std::vector<std::string>> authority_options_ =
      std::make_shared<std::vector<std::string>>();
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H
