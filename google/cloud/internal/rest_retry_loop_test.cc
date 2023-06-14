// Copyright 2023 Google LLC
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

#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockBackoffPolicy;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

struct StringOption {
  using Type = std::string;
};

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

auto constexpr kNumRetries = 3;

std::unique_ptr<internal::RetryPolicy> TestRetryPolicy() {
  return internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>(
             kNumRetries)
      .clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

TEST(RestRetryLoopTest, Success) {
  internal::OptionsSpan span(Options{}.set<StringOption>("Success"));
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, int request) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(), "Success");
        return StatusOr<int>(2 * request);
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RestRetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  internal::OptionsSpan span(
      Options{}.set<StringOption>("TransientThenSuccess"));
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [&counter](RestContext&, int request) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "TransientThenSuccess");
        if (++counter < 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RestRetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  internal::OptionsSpan span(Options{}.set<StringOption>("ReturnJustStatus"));
  Status actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [&counter](RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "ReturnJustStatus");
        if (++counter <= 3) {
          return Status(StatusCode::kResourceExhausted, "slow-down");
        }
        return Status();
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
}

/**
 * @test Verify the backoff policy is queried after each failure.
 *
 * Note that this is not testing if the
 */
TEST(RestRetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion())
      .WillOnce(Return(ms(10)))
      .WillOnce(Return(ms(20)))
      .WillOnce(Return(ms(30)));

  int counter = 0;
  std::vector<ms> sleep_for;
  internal::OptionsSpan span(Options{}.set<StringOption>("UsesBackoffPolicy"));
  StatusOr<int> actual = RestRetryLoopImpl(
      TestRetryPolicy(), std::move(mock), Idempotency::kIdempotent,
      [&counter](RestContext&, int request) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "UsesBackoffPolicy");
        if (++counter <= 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      42, "error message", [&sleep_for](ms p) { sleep_for.push_back(p); });
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
  EXPECT_THAT(sleep_for,
              ElementsAre(ms(10), std::chrono::milliseconds(20), ms(30)));
}

TEST(RestRetryLoopTest, TransientFailureNonIdempotent) {
  internal::OptionsSpan span(
      Options{}.set<StringOption>("TransientFailureNonIdempotent"));
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
      [](RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "TransientFailureNonIdempotent");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      42, "the answer to everything");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Error in non-idempotent"));
}

TEST(RestRetryLoopTest, PermanentFailureFailureIdempotent) {
  internal::OptionsSpan span(
      Options{}.set<StringOption>("PermanentFailureFailureIdempotent"));
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PermanentFailureFailureIdempotent");
        return StatusOr<int>(Status(StatusCode::kPermissionDenied, "uh oh"));
      },
      42, "the answer to everything");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("uh oh"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Permanent error"));
}

TEST(RestRetryLoopTest, TooManyTransientFailuresIdempotent) {
  internal::OptionsSpan span(
      Options{}.set<StringOption>("TransientFailureNonIdempotent"));
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "TransientFailureNonIdempotent");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      42, "the answer to everything");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Retry policy exhausted"));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(RestRetryLoopTest, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  internal::OptionsSpan o(EnableTracing(Options{}));

  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, int) {
        return StatusOr<int>(internal::UnavailableError("try again"));
      },
      0, "error message");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, AllOf(SizeIs(kNumRetries), Each(SpanNamed("Backoff"))));
}

TEST(RestRetryLoopTest, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  internal::OptionsSpan o(DisableTracing(Options{}));

  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, int) { return StatusOr<int>(0); }, 0, "error message");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
