// Copyright 2019 Google LLC
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

#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
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

std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return LimitedErrorCountRetryPolicy<TestRetryablePolicy>(kNumRetries).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

TEST(RetryLoopTest, Success) {
  OptionsSpan span(Options{}.set<StringOption>("Success"));
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext&, int request) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "Success");
        return StatusOr<int>(2 * request);
      },
      42, "error message");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  OptionsSpan span(Options{}.set<StringOption>("TransientThenSuccess"));
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [&counter](grpc::ClientContext&, int request) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "TransientThenSuccess");
        if (++counter < 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      42, "error message");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  OptionsSpan span(Options{}.set<StringOption>("ReturnJustStatus"));
  Status actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [&counter](grpc::ClientContext&, int) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "ReturnJustStatus");
        if (++counter <= 3) {
          return Status(StatusCode::kResourceExhausted, "slow-down");
        }
        return Status();
      },
      42, "error message");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
}

/**
 * @test Verify the backoff policy is queried after each failure.
 *
 * Note that this is not testing if the
 */
TEST(RetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion())
      .WillOnce(Return(ms(10)))
      .WillOnce(Return(ms(20)))
      .WillOnce(Return(ms(30)));

  int counter = 0;
  std::vector<ms> sleep_for;
  OptionsSpan span(Options{}.set<StringOption>("UsesBackoffPolicy"));
  StatusOr<int> actual = RetryLoopImpl(
      TestRetryPolicy(), std::move(mock), Idempotency::kIdempotent,
      [&counter](grpc::ClientContext&, int request) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "UsesBackoffPolicy");
        if (++counter <= 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      42, "error message", [&sleep_for](ms p) { sleep_for.push_back(p); });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
  EXPECT_THAT(sleep_for,
              ElementsAre(ms(10), std::chrono::milliseconds(20), ms(30)));
}

TEST(RetryLoopTest, TransientFailureNonIdempotent) {
  OptionsSpan span(
      Options{}.set<StringOption>("TransientFailureNonIdempotent"));
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
      [](grpc::ClientContext&, int) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "TransientFailureNonIdempotent");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      42, "the answer to everything");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Error in non-idempotent"));
}

TEST(RetryLoopTest, PermanentFailureFailureIdempotent) {
  OptionsSpan span(
      Options{}.set<StringOption>("PermanentFailureFailureIdempotent"));
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext&, int) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PermanentFailureFailureIdempotent");
        return StatusOr<int>(Status(StatusCode::kPermissionDenied, "uh oh"));
      },
      42, "the answer to everything");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("uh oh"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Permanent error"));
}

TEST(RetryLoopTest, TooManyTransientFailuresIdempotent) {
  OptionsSpan span(
      Options{}.set<StringOption>("TransientFailureNonIdempotent"));
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext&, int) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "TransientFailureNonIdempotent");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      42, "the answer to everything");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Retry policy exhausted"));
}

TEST(RetryLoopTest, ExhaustedOnStart) {
  internal::OptionsSpan span(Options{}.set<StringOption>("ExhaustedOnStart"));
  auto retry_policy = internal::LimitedTimeRetryPolicy<TestRetryablePolicy>(
      std::chrono::seconds(0));
  ASSERT_TRUE(retry_policy.IsExhausted());
  StatusOr<int> actual = RetryLoop(
      retry_policy.clone(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "ExhaustedOnStart");
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      42, "the answer to everything");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kDeadlineExceeded,
                               HasSubstr("Retry policy exhausted before")));
}

TEST(RetryLoopTest, ConfigureContext) {
  auto setup = [](grpc::ClientContext& context) {
    context.set_compression_algorithm(GRPC_COMPRESS_DEFLATE);
  };
  OptionsSpan span(Options{}
                       .set<StringOption>("ConfigureContext")
                       .set<internal::GrpcSetupOption>(setup));

  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext& context, int) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "ConfigureContext");
        // Ensure that our options have taken affect on the ClientContext
        // before we start using it.
        EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context.compression_algorithm());
        return StatusOr<int>(0);
      },
      0, "error message");
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(RetryLoopTest, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  OptionsSpan o(EnableTracing(Options{}));

  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext&, int) {
        return StatusOr<int>(UnavailableError("try again"));
      },
      0, "error message");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, AllOf(SizeIs(kNumRetries), Each(SpanNamed("Backoff"))));
}

TEST(RetryLoopTest, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  OptionsSpan o(DisableTracing(Options{}));

  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      [](grpc::ClientContext&, int) { return StatusOr<int>(0); }, 0,
      "error message");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
