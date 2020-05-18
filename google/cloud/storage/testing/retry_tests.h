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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H

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
 * - Verify an error status is returned, with the right contents.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The low-level RawClient return type (modulo StatusOr).
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 */
template <typename ReturnType, typename F>
void TooManyFailuresStatusTest(
    std::shared_ptr<testing::MockClient> const& mock,
    ::testing::internal::TypedExpectation<F>& oncall,
    std::function<Status(Client& client)> const& tested_operation,
    char const* api_name) {
  using ::google::cloud::storage::testing::canonical_errors::TransientError;
  using ::testing::HasSubstr;
  using ::testing::Return;
  // A storage::Client with a simple to test policy.
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1), 2.0)};

  // Expect exactly 3 calls before the retry policy is exhausted and an error
  // status is returned.
  oncall.WillOnce(Return(StatusOr<ReturnType>(TransientError())))
      .WillOnce(Return(StatusOr<ReturnType>(TransientError())))
      .WillOnce(Return(StatusOr<ReturnType>(TransientError())));

  Status status = tested_operation(client);
  EXPECT_EQ(TransientError().code(), status.code());
  EXPECT_THAT(status.message(), HasSubstr("Retry policy exhausted"));
  EXPECT_THAT(status.message(), HasSubstr(api_name));
}

/**
 * Tests that non-idempotent operations are *not* retried case for a `Client::*`
 * member function.
 *
 * We need to verify that non-idempotent operations are not retied when the
 * policy says so. The tests are quite repetitive, and all have the same
 * structure:
 *
 * - Create a `storage::Client` with the right idempotency policies.
 * - Setup the mock to return the right number of transient failures.
 * - Call the API.
 * - Verify an error status is returned, with the right contents.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The low-level RawClient return type (modulo StatusOr).
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 */
template <typename ReturnType, typename F>
void NonIdempotentFailuresStatusTest(
    std::shared_ptr<testing::MockClient> const& mock,
    ::testing::internal::TypedExpectation<F>& oncall,
    std::function<Status(Client& client)> const& tested_operation,
    char const* api_name) {
  using ::google::cloud::storage::testing::canonical_errors::TransientError;
  using ::testing::HasSubstr;
  using ::testing::Return;
  // A storage::Client with the strict idempotency policy, but with a generous
  // retry policy.
  Client client{std::shared_ptr<internal::RawClient>(mock),
                StrictIdempotencyPolicy(), LimitedErrorCountRetryPolicy(10),
                ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1), 2.0)};

  // The first transient error should stop the retries for non-idempotent
  // operations.
  oncall.WillOnce(Return(StatusOr<ReturnType>(TransientError())));

  // Verify the right error status, with the right content, is returned when
  // calling the operation.
  Status status = tested_operation(client);
  EXPECT_EQ(status.code(), TransientError().code());
  EXPECT_THAT(status.message(), HasSubstr("Error in non-idempotent"));
  EXPECT_THAT(status.message(), HasSubstr(api_name));
}

/**
 * Tests that idempotent operations *are* retried for a `Client::*` member
 * function.
 *
 * We need to verify that idempotent operations are retried when the policy says
 * so. The tests are quite repetitive, and all have the same structure:
 *
 * - Create a `storage::Client` with the right idempotency policies.
 * - Setup the mock to return the right number of transient failures.
 * - Call the API.
 * - Verify an error status is returned, with the right contents.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The low-level RawClient return type (modulo StatusOr).
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 */
template <typename ReturnType, typename F>
void IdempotentFailuresStatusTest(
    std::shared_ptr<testing::MockClient> const& mock,
    ::testing::internal::TypedExpectation<F>& oncall,
    std::function<Status(Client& client)> const& tested_operation,
    char const* api_name) {
  using ::google::cloud::storage::testing::canonical_errors::TransientError;
  using ::testing::HasSubstr;
  using ::testing::Return;
  // A storage::Client with the strict idempotency policy, and with an
  // easy-to-test retry policy.
  Client client{std::shared_ptr<internal::RawClient>(mock),
                StrictIdempotencyPolicy(), LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1), 2.0)};

  // Expect exactly 3 calls before the retry policy is exhausted and an error
  // status is returned.
  oncall.WillOnce(Return(StatusOr<ReturnType>(TransientError())))
      .WillOnce(Return(StatusOr<ReturnType>(TransientError())))
      .WillOnce(Return(StatusOr<ReturnType>(TransientError())));

  // Verify the right error status, with the right content, is returned when
  // calling the operation.
  Status status = tested_operation(client);
  EXPECT_EQ(TransientError().code(), status.code());
  EXPECT_THAT(status.message(), HasSubstr("Retry policy exhausted"));
  EXPECT_THAT(status.message(), HasSubstr(api_name));
}

/**
 * Test operations that are idempotent or not depending of their parameters.
 *
 * Some operations are idempotent when some parameters are set (preconditions).
 * When the operation is idempotent, we want to verify that they are retried
 * multiple times, when they are not, we want to retry them only once if the
 * right policy is set.
 *
 * - Create a `storage::Client` with an easy-to-test retry policy.
 * - Setup the mock to return the right number of transient failures.
 * - Call the API.
 * - Verify an error status is returned, with the right contents.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The low-level RawClient return type (modulo StatusOr).
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 */
template <typename ReturnType, typename F>
void TooManyFailuresStatusTest(
    std::shared_ptr<testing::MockClient> const& mock,
    ::testing::internal::TypedExpectation<F>& oncall,
    std::function<Status(Client& client)> const& tested_operation,
    std::function<Status(Client& client)> const& idempotent_operation,
    char const* api_name) {
  TooManyFailuresStatusTest<ReturnType>(mock, oncall, tested_operation,
                                        api_name);
  IdempotentFailuresStatusTest<ReturnType>(mock, oncall, idempotent_operation,
                                           api_name);
  NonIdempotentFailuresStatusTest<ReturnType>(mock, oncall, tested_operation,
                                              api_name);
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
 * - Verify an error status is returned, with the right contents.
 *
 * This function implements these tests, saving us a lot of repetitive code.
 *
 * @tparam ReturnType The low-level RawClient return type (modulo StatusOr).
 * @tparam F a formal parameter, the googlemock representation of the mocked
 *     call.
 *
 * @param oncall The internal type returned by EXPECT_CALL(...).
 * @param tested_operation a function wrapping the operation to be tested.
 * @param api_name the name of the api
 */
template <typename ReturnType, typename F>
void PermanentFailureStatusTest(
    Client& client, ::testing::internal::TypedExpectation<F>& oncall,
    std::function<Status(Client& client)> const& tested_operation,
    char const* api_name) {
  using ::google::cloud::storage::testing::canonical_errors::PermanentError;
  using ::testing::HasSubstr;
  using ::testing::Return;

  // Expect exactly one call before the retry policy is exhausted and an error
  // status is returned.
  oncall.WillOnce(Return(StatusOr<ReturnType>(PermanentError())));

  // Verify the right exception type, with the right content, is raised when
  // calling the operation.
  Status status = tested_operation(client);
  EXPECT_EQ(PermanentError().code(), status.code());
  EXPECT_THAT(status.message(), HasSubstr("Permanent error"));
  EXPECT_THAT(status.message(), HasSubstr(api_name));
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_TESTS_H
