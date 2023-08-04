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

#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/**
 * Returns options used in the `RetryClient` tests.
 *
 * These options set the retry policy to accept at most 2 transient errors. The
 * backoff policy is uses very short backoffs. This works will in unit tests.
 * And the idempotency policy retries all operations.
 */
Options RetryClientTestOptions();

/// Validates the `Status` produced in a "too many transients" test.
::testing::Matcher<Status> StoppedOnTooManyTransients(char const* api_name);

/// Validates the `Status` produced in a "permanent error" test.
::testing::Matcher<Status> StoppedOnPermanentError(char const* api_name);

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H
