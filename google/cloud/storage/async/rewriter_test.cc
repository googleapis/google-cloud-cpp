// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/rewriter.h"
#include "google/cloud/storage/mocks/mock_async_rewriter_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_mocks::MockAsyncRewriterConnection;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::google::storage::v2::RewriteResponse;
using ::testing::AllOf;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::ResultOf;

TEST(AsyncRewriter, Basic) {
  auto mock = std::make_shared<MockAsyncRewriterConnection>();
  EXPECT_CALL(*mock, Iterate)
      .WillOnce([] {
        RewriteResponse response;
        response.set_total_bytes_rewritten(1000);
        response.set_object_size(3000);
        response.set_rewrite_token("test-rewrite-token");
        return make_ready_future(make_status_or(std::move(response)));
      })
      .WillOnce([] {
        RewriteResponse response;
        response.set_total_bytes_rewritten(3000);
        response.set_object_size(3000);
        response.mutable_resource()->set_size(3000);
        return make_ready_future(make_status_or(std::move(response)));
      });

  AsyncRewriter rewriter(mock);
  auto i1 =
      rewriter.Iterate(storage_internal::MakeAsyncToken(mock.get())).get();
  EXPECT_THAT(
      i1,
      IsOkAndHolds(Pair(
          AllOf(ResultOf(
                    "total bytes",
                    [](RewriteResponse const& v) {
                      return v.total_bytes_rewritten();
                    },
                    1000),
                ResultOf(
                    "size",
                    [](RewriteResponse const& v) { return v.object_size(); },
                    3000),
                ResultOf(
                    "token",
                    [](RewriteResponse const& v) { return v.rewrite_token(); },
                    "test-rewrite-token"),
                ResultOf(
                    "has resource",
                    [](RewriteResponse const& v) { return v.has_resource(); },
                    false)),
          ResultOf(
              "token is valid", [](auto const& t) { return t.valid(); },
              true))));

  auto i2 =
      rewriter.Iterate(storage_internal::MakeAsyncToken(mock.get())).get();
  EXPECT_THAT(
      i2,
      IsOkAndHolds(Pair(
          AllOf(
              ResultOf(
                  "total bytes",
                  [](RewriteResponse const& v) {
                    return v.total_bytes_rewritten();
                  },
                  3000),
              ResultOf(
                  "size",
                  [](RewriteResponse const& v) { return v.object_size(); },
                  3000),
              ResultOf(
                  "token",
                  [](RewriteResponse const& v) { return v.rewrite_token(); },
                  IsEmpty()),
              ResultOf(
                  "has resource",
                  [](RewriteResponse const& v) { return v.has_resource(); },
                  true),
              ResultOf(
                  "metadata.size",
                  [](RewriteResponse const& v) { return v.resource().size(); },
                  3000)),
          ResultOf(
              "token is valid", [](auto const& t) { return t.valid(); },
              false))));
}

TEST(AsyncRewriter, WithError) {
  auto mock = std::make_shared<MockAsyncRewriterConnection>();
  EXPECT_CALL(*mock, Iterate).WillOnce([] {
    return make_ready_future(StatusOr<RewriteResponse>(PermanentError()));
  });

  AsyncRewriter rewriter(mock);
  auto const actual =
      rewriter.Iterate(storage_internal::MakeAsyncToken(mock.get())).get();
  EXPECT_THAT(actual, StatusIs(PermanentError().code()));
}

TEST(AsyncRewriter, ErrorOnDefaultConstructed) {
  AsyncRewriter rewriter;
  auto const actual =
      rewriter.Iterate(storage_internal::MakeAsyncToken(&rewriter)).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
}

TEST(AsyncRewriter, ErrorWithInvalidToken) {
  auto mock = std::make_unique<MockAsyncRewriterConnection>();
  EXPECT_CALL(*mock, Iterate).Times(0);

  AsyncRewriter rewriter(std::move(mock));
  auto const actual = rewriter.Iterate(AsyncToken()).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST(AsyncRewriter, ErrorWithMismatchedToken) {
  auto mock = std::make_unique<MockAsyncRewriterConnection>();
  EXPECT_CALL(*mock, Iterate).Times(0);

  int placeholder = 0;
  AsyncRewriter rewriter(std::move(mock));
  auto const actual =
      rewriter.Iterate(storage_internal::MakeAsyncToken(&placeholder)).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
