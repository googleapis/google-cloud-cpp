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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H_

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
/**
 * Tests the "too many failures" case for a `Client::*` member function.
 *
 * We need to verify that each API in the client library handles "too many
 * errors" correctly. The tests are quite repetitive, and all have the same
 * structure:
 *
 * - Create a `storage::Client` with an easy-to-test retry policy.
 * - Setup the mock to return the right number of transient failures.
 * - Call the API.
 * - Verify an exception was raised, with the right messages.
 * - Also implement the last 3 steps for the no-exceptions case.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The API return type.
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 * @return a function that implements the test.
 */
template <typename ReturnType, typename F>
void TooManyFailuresTest(std::shared_ptr<testing::MockClient> mock,
                         ::testing::internal::TypedExpectation<F>& oncall,
                         std::function<void(Client& client)> tested_operation,
                         char const* api_name) {
  using canonical_errors::TransientError;
  using ::testing::HasSubstr;
  using ::testing::Return;
  // A storage::Client with a simple to test policy.
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Expect exactly 3 calls before the retry policy is exhausted and an
  // exception is raised.
  oncall.WillOnce(Return(std::make_pair(TransientError(), ReturnType{})))
      .WillOnce(Return(std::make_pair(TransientError(), ReturnType{})))
      .WillOnce(Return(std::make_pair(TransientError(), ReturnType{})));

  // Verify the right exception type, with the right content, is raised when
  // calling the operation.
  EXPECT_THROW(
      try { tested_operation(client); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Retry policy exhausted"));
        EXPECT_THAT(ex.what(), HasSubstr(api_name));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times
  // the operation is called, so we cannot set an exact number of calls to
  // expect.
  oncall.WillRepeatedly(Return(std::make_pair(TransientError(), ReturnType{})));
  EXPECT_DEATH_IF_SUPPORTED(tested_operation(client),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * Tests the "permanent failure" case for a `Client::*` member function.
 *
 * We need to verify that each API in the client library handles permanent
 * failures correctly. The tests are quite repetitive, and all have the same
 * structure:
 *
 * - Setup the mock to return a permanent failure.
 * - Call the API.
 * - Verify an exception was raised, with the right messages.
 * - Also implement the last 3 steps for the no-exceptions case.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The API return type.
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 * @return a function that implements the test.
 */
template <typename ReturnType, typename F>
void PermanentFailureTest(Client& client,
                          ::testing::internal::TypedExpectation<F>& oncall,
                          std::function<void(Client& client)> tested_operation,
                          char const* api_name) {
  using canonical_errors::PermanentError;
  using ::testing::HasSubstr;
  using ::testing::Return;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Expect exactly one call before the retry policy is exhausted and an
  // exception is raised.
  oncall.WillOnce(Return(std::make_pair(PermanentError(), ReturnType{})));

  // Verify the right exception type, with the right content, is raised when
  // calling the operation.
  EXPECT_THROW(
      try { tested_operation(client); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Permanent error"));
        EXPECT_THAT(ex.what(), HasSubstr(api_name));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times
  // the operation is called, so we cannot set an exact number of calls to
  // expect.
  oncall.WillRepeatedly(Return(std::make_pair(PermanentError(), ReturnType{})));
  EXPECT_DEATH_IF_SUPPORTED(tested_operation(client),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H_
