// Copyright 2021 Google LLC
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

#include "google/cloud/internal/grpc_async_access_token_cache.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;

TEST(GrpcAsyncAccessTokenCacheTest, Simple) {
  ::testing::MockFunction<future<StatusOr<AccessToken>>(CompletionQueue&)>
      mock_source;
  auto const start = std::chrono::system_clock::now();
  using minutes = std::chrono::minutes;
  auto const t1 = AccessToken{"token1", start + minutes(10)};
  auto const t2 = AccessToken{"token2", start + minutes(20)};

  AsyncSequencer<void> async;
  EXPECT_CALL(mock_source, Call)
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) { return make_status_or(t1); });
      })
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) { return make_status_or(t2); });
      });

  AutomaticallyCreatedBackgroundThreads background;
  auto under_test = GrpcAsyncAccessTokenCache::Create(
      background.cq(), mock_source.AsStdFunction());

  auto pending = under_test->AsyncGetAccessToken(start);
  async.PopFront().set_value();
  EXPECT_THAT(pending.get(), IsOk());

  // For the next few minutes the cache makes no further calls.
  for (auto m : {minutes(1), minutes(2), minutes(3)}) {
    SCOPED_TRACE("Testing at start + " + std::to_string(m.count()) + "m");
    auto r = under_test->GetAccessToken(start + m);
    ASSERT_THAT(r, IsOk());
    EXPECT_EQ(t1.token, r->token);
    EXPECT_EQ(t1.expiration, r->expiration);
  }

  // At start+6m the cache makes a call, but still returns the cached value.
  auto r = under_test->GetAccessToken(start + minutes(6));
  ASSERT_THAT(r, IsOk());
  EXPECT_EQ(t1.token, r->token);
  EXPECT_EQ(t1.expiration, r->expiration);

  // Have the async operation complete and test at start+11m
  async.PopFront().set_value();
  r = under_test->GetAccessToken(start + minutes(11));
  ASSERT_THAT(r, IsOk());
  EXPECT_EQ(t2.token, r->token);
  EXPECT_EQ(t2.expiration, r->expiration);
}

TEST(GrpcAsyncAccessTokenCacheTest, SimpleAsync) {
  ::testing::MockFunction<future<StatusOr<AccessToken>>(CompletionQueue&)>
      mock_source;
  auto const start = std::chrono::system_clock::now();
  using minutes = std::chrono::minutes;
  auto const t1 = AccessToken{"token1", start + minutes(10)};
  auto const t2 = AccessToken{"token2", start + minutes(20)};

  AsyncSequencer<void> async;
  EXPECT_CALL(mock_source, Call)
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) { return make_status_or(t1); });
      })
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) { return make_status_or(t2); });
      });

  AutomaticallyCreatedBackgroundThreads background;
  auto under_test = GrpcAsyncAccessTokenCache::Create(
      background.cq(), mock_source.AsStdFunction());

  auto pending = under_test->AsyncGetAccessToken(start);
  async.PopFront().set_value();
  EXPECT_THAT(pending.get(), IsOk());

  // For the next few minutes the cache makes no further calls.
  for (auto m : {minutes(1), minutes(2), minutes(3)}) {
    SCOPED_TRACE("Testing at start + " + std::to_string(m.count()) + "m");
    auto r = under_test->AsyncGetAccessToken(start + m).get();
    ASSERT_THAT(r, IsOk());
    EXPECT_EQ(t1.token, r->token);
    EXPECT_EQ(t1.expiration, r->expiration);
  }

  // At start+6m the cache makes a call, but still returns the cached value.
  auto r = under_test->AsyncGetAccessToken(start + minutes(6)).get();
  ASSERT_THAT(r, IsOk());
  EXPECT_EQ(t1.token, r->token);
  EXPECT_EQ(t1.expiration, r->expiration);

  // Have the async operation complete and test at start+11m
  async.PopFront().set_value();
  r = under_test->AsyncGetAccessToken(start + minutes(11)).get();
  ASSERT_THAT(r, IsOk());
  EXPECT_EQ(t2.token, r->token);
  EXPECT_EQ(t2.expiration, r->expiration);
}

TEST(GrpcAsyncAccessTokenCacheTest, IgnoreErrorsOnPreCaching) {
  ::testing::MockFunction<future<StatusOr<AccessToken>>(CompletionQueue&)>
      mock_source;
  auto const start = std::chrono::system_clock::now();
  using minutes = std::chrono::minutes;
  auto const t1 = AccessToken{"token1", start + minutes(10)};
  auto const t2 = AccessToken{"token2", start + minutes(20)};

  AsyncSequencer<void> async;
  EXPECT_CALL(mock_source, Call)
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) { return make_status_or(t1); });
      })
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) -> StatusOr<AccessToken> {
              return Status(StatusCode::kUnavailable, "try-again");
            });
      })
      .WillOnce([&](CompletionQueue&) {
        return async.PushBack().then(
            [&](future<void>) { return make_status_or(t2); });
      });

  AutomaticallyCreatedBackgroundThreads background;
  auto under_test = GrpcAsyncAccessTokenCache::Create(
      background.cq(), mock_source.AsStdFunction());

  auto pending = under_test->AsyncGetAccessToken(start);
  async.PopFront().set_value();
  EXPECT_THAT(pending.get(), IsOk());

  // At start+5m the cache will do an early pre-fetch, but use the current
  // value until the pre-fetch completes.
  using std::chrono::seconds;
  for (auto s : {seconds(1), seconds(2), seconds(3)}) {
    auto i = minutes(5) + s;
    SCOPED_TRACE("Testing at start + " + std::to_string(i.count()) + "s");
    auto r = under_test->GetAccessToken(start + i);
    ASSERT_THAT(r, IsOk());
    EXPECT_EQ(r->token, t1.token);
  }

  // This will simulate the completion of the pre-fetch, the mock setup returns
  // a failure, that should leave the status unchanged:
  async.PopFront().set_value();
  for (auto s : {seconds(4), seconds(5), seconds(6)}) {
    auto i = minutes(5) + s;
    SCOPED_TRACE("Testing at start + " + std::to_string(i.count()) + "s");
    auto r = under_test->GetAccessToken(start + i);
    ASSERT_THAT(r, IsOk());
    EXPECT_EQ(r->token, t1.token);
  }

  // The previous requests should have triggered another pre-fetch. Completing
  // the request should trigger a successful value.
  async.PopFront().set_value();
  for (auto s : {seconds(7), seconds(8)}) {
    auto i = minutes(5) + s;
    SCOPED_TRACE("Testing at start + " + std::to_string(i.count()) + "s");
    auto r = under_test->GetAccessToken(start + i);
    ASSERT_THAT(r, IsOk());
    EXPECT_EQ(t2.token, r->token);
  }
}

TEST(GrpcAsyncAccessTokenCacheTest, ReturnError) {
  ::testing::MockFunction<future<StatusOr<AccessToken>>(CompletionQueue&)>
      mock_source;
  auto const start = std::chrono::system_clock::now();

  AsyncSequencer<void> async;
  EXPECT_CALL(mock_source, Call).WillOnce([&](CompletionQueue&) {
    return async.PushBack().then([&](future<void>) -> StatusOr<AccessToken> {
      return Status(StatusCode::kUnavailable, "try-again");
    });
  });

  AutomaticallyCreatedBackgroundThreads background;
  auto under_test = GrpcAsyncAccessTokenCache::Create(
      background.cq(), mock_source.AsStdFunction());

  auto pending = under_test->AsyncGetAccessToken(start);
  async.PopFront().set_value();
  EXPECT_THAT(pending.get(), StatusIs(StatusCode::kUnavailable, "try-again"));
}

TEST(GrpcAsyncAccessTokenCacheTest, SatisfyMany) {
  ::testing::MockFunction<future<StatusOr<AccessToken>>(CompletionQueue&)>
      mock_source;
  auto const start = std::chrono::system_clock::now();
  using minutes = std::chrono::minutes;
  auto const t1 = AccessToken{"token1", start + minutes(10)};

  AsyncSequencer<void> async;
  EXPECT_CALL(mock_source, Call).WillOnce([&](CompletionQueue&) {
    return async.PushBack().then(
        [&](future<void>) { return make_status_or(t1); });
  });

  AutomaticallyCreatedBackgroundThreads background;
  auto under_test = GrpcAsyncAccessTokenCache::Create(
      background.cq(), mock_source.AsStdFunction());

  std::vector<future<StatusOr<AccessToken>>> results(3);
  std::generate(results.begin(), results.end(),
                [&] { return under_test->AsyncGetAccessToken(start); });

  // Making multiple requests creates one refresh attempt, simulate its
  // completion, that should satisfy all the pending requests.
  async.PopFront().set_value();
  for (auto& f : results) {
    auto r = f.get();
    ASSERT_THAT(r, IsOk());
    ASSERT_EQ(t1.token, r->token);
    ASSERT_EQ(t1.expiration, r->expiration);
  }
}

TEST(GrpcAsyncAccessTokenCacheTest, BlockingRefresh) {
  ::testing::MockFunction<future<StatusOr<AccessToken>>(CompletionQueue&)>
      mock_source;
  auto const start = std::chrono::system_clock::now();
  using minutes = std::chrono::minutes;
  auto const t1 = AccessToken{"token1", start + minutes(10)};

  EXPECT_CALL(mock_source, Call).WillOnce([&](CompletionQueue&) {
    return make_ready_future(make_status_or(t1));
  });

  AutomaticallyCreatedBackgroundThreads background;
  auto under_test = GrpcAsyncAccessTokenCache::Create(
      background.cq(), mock_source.AsStdFunction());

  auto r = under_test->GetAccessToken(start);
  EXPECT_THAT(r, IsOk());
  EXPECT_EQ(t1.token, r->token);
  EXPECT_EQ(t1.expiration, r->expiration);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
