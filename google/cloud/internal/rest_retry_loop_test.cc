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
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_policy_impl.h"
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

using ::google::cloud::Idempotency;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Pair;
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

std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>(
             kNumRetries)
      .clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                  std::chrono::milliseconds(5), 2.0)
      .clone();
}

TEST(RestRetryLoopTest, Success) {
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext& context, Options const& options, int request) {
        EXPECT_EQ(options.get<StringOption>(), "Success");
        EXPECT_EQ(context.options().get<StringOption>(), "Success");
        return StatusOr<int>(2 * request);
      },
      Options{}.set<StringOption>("Success"), /*request=*/42,
      /*location=*/"error message");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RestRetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [&counter](RestContext& context, Options const& options, int request) {
        EXPECT_EQ(options.get<StringOption>(), "TransientThenSuccess");
        EXPECT_EQ(context.options().get<StringOption>(),
                  "TransientThenSuccess");
        if (++counter < 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      Options{}.set<StringOption>("TransientThenSuccess"), /*request=*/42,
      /*location=*/"error message");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RestRetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  Status actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [&counter](RestContext& context, Options const& options, int) {
        EXPECT_EQ(options.get<StringOption>(), "ReturnJustStatus");
        EXPECT_EQ(context.options().get<StringOption>(), "ReturnJustStatus");
        if (++counter <= 3) {
          return Status(StatusCode::kResourceExhausted, "slow-down");
        }
        return Status();
      },
      Options{}.set<StringOption>("ReturnJustStatus"), /*request=*/42,
      /*location=*/"error message");
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
  auto retry_policy = TestRetryPolicy();
  StatusOr<int> actual = RestRetryLoopImpl(
      *retry_policy, *mock, Idempotency::kIdempotent,
      [&counter](RestContext&, Options const&, int request) {
        if (++counter <= 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      Options{}, /*request=*/42,
      /*location=*/"error message",
      /*sleeper=*/[&sleep_for](ms p) { sleep_for.push_back(p); });
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
  EXPECT_THAT(sleep_for,
              ElementsAre(ms(10), std::chrono::milliseconds(20), ms(30)));
}

TEST(RestRetryLoopTest, TransientFailureNonIdempotent) {
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
      [](RestContext&, Options const& options, int) {
        EXPECT_EQ(options.get<StringOption>(), "TransientFailureNonIdempotent");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      Options{}.set<StringOption>("TransientFailureNonIdempotent"),
      /*request=*/42, /*location=*/__func__);
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.original-message", "try again")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.reason", "non-idempotent")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", __func__)));
}

TEST(RestRetryLoopTest, PermanentFailureFailureIdempotent) {
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, Options const& options, int) {
        EXPECT_EQ(options.get<StringOption>(),
                  "PermanentFailureFailureIdempotent");
        return StatusOr<int>(Status(StatusCode::kPermissionDenied, "uh oh"));
      },
      Options{}.set<StringOption>("PermanentFailureFailureIdempotent"),
      /*request=*/42, /*location=*/__func__);
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("uh oh"));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.original-message", "uh oh")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.reason", "permanent-error")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", __func__)));
}

TEST(RestRetryLoopTest, TooManyTransientFailuresIdempotent) {
  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, Options const& options, int) {
        EXPECT_EQ(options.get<StringOption>(),
                  "TooManyTransientFailuresIdempotent");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      Options{}.set<StringOption>("TooManyTransientFailuresIdempotent"),
      /*request=*/42, /*location=*/__func__);
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.original-message", "try again")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.on-entry", "false")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", __func__)));
}

TEST(RestRetryLoopTest, ExhaustedOnStart) {
  auto retry_policy = internal::LimitedTimeRetryPolicy<TestRetryablePolicy>(
      std::chrono::seconds(0));
  ASSERT_TRUE(retry_policy.IsExhausted());
  StatusOr<int> actual = RestRetryLoop(
      retry_policy.clone(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, Options const& options, int) {
        EXPECT_EQ(options.get<StringOption>(), "ExhaustedOnStart");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      Options{}.set<StringOption>("ExhaustedOnStart"), /*request=*/42,
      /*location=*/__func__);
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kDeadlineExceeded));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.on-entry", "true")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", __func__)));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(RestRetryLoopTest, TracingEnabledExplicitOptions) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  auto const options = EnableTracing(Options{});

  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, Options const&, int) {
        return StatusOr<int>(internal::UnavailableError("try again"));
      },
      options, /*request=*/42, /*location=*/"error message");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, AllOf(SizeIs(kNumRetries), Each(SpanNamed("Backoff"))));
}

TEST(RestRetryLoopTest, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  StatusOr<int> actual = RestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](RestContext&, Options const&, int) { return StatusOr<int>(0); },
      /*options=*/DisableTracing(Options{}), /*request=*/0,
      /*location=*/"error message");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
