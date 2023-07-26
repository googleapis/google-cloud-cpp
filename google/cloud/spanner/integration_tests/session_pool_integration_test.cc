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

#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/spanner_stub_factory.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct SessionPoolFriendForTest {
  static future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(std::shared_ptr<SessionPool> const& session_pool,
                           CompletionQueue& cq,
                           std::shared_ptr<SpannerStub> const& stub,
                           std::map<std::string, std::string> const& labels,
                           std::string const& role, int num_sessions) {
    internal::OptionsSpan span(session_pool->opts_);
    return session_pool->AsyncBatchCreateSessions(cq, stub, labels, role,
                                                  num_sessions);
  }

  static future<Status> AsyncDeleteSession(
      std::shared_ptr<SessionPool> const& session_pool, CompletionQueue& cq,
      std::shared_ptr<SpannerStub> const& stub, std::string session_name) {
    internal::OptionsSpan span(session_pool->opts_);
    return session_pool->AsyncDeleteSession(cq, stub, std::move(session_name));
  }

  static future<StatusOr<google::spanner::v1::ResultSet>> AsyncRefreshSession(
      std::shared_ptr<SessionPool> const& session_pool, CompletionQueue& cq,
      std::shared_ptr<SpannerStub> const& stub, std::string session_name) {
    internal::OptionsSpan span(session_pool->opts_);
    return session_pool->AsyncRefreshSession(cq, stub, std::move(session_name));
  }
};

namespace {

class SessionPoolIntegrationTest
    : public spanner_testing::DatabaseIntegrationTest {};

TEST_F(SessionPoolIntegrationTest, SessionAsyncCRUD) {
  google::cloud::CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });
  auto const db = GetDatabase();
  auto opts = spanner_internal::DefaultOptions();
  opts.set<spanner::SpannerRetryPolicyOption>(
      std::make_shared<spanner::LimitedTimeRetryPolicy>(
          std::chrono::minutes(5)));
  opts.set<spanner::SpannerBackoffPolicyOption>(
      std::make_shared<spanner::ExponentialBackoffPolicy>(
          std::chrono::seconds(10), std::chrono::minutes(1), 2.0));

  auto stub = CreateDefaultSpannerStub(
      db, internal::CreateAuthenticationStrategy(cq, opts), opts,
      /*channel_id=*/0);
  auto session_pool = MakeSessionPool(db, {stub}, cq, opts);

  // Make an asynchronous request, but immediately block until the response
  // arrives
  auto constexpr kSessionCreatorRole = "public";
  auto constexpr kNumTestSession = 4;
  auto create_response =
      spanner_internal::SessionPoolFriendForTest::AsyncBatchCreateSessions(
          session_pool, cq, stub, {}, kSessionCreatorRole, kNumTestSession)
          .get();
  ASSERT_STATUS_OK(create_response);
  EXPECT_EQ(kNumTestSession, create_response->session_size());

  std::vector<future<bool>> async_get;
  for (auto const& s : create_response->session()) {
    auto const& session_name = s.name();
    async_get.push_back(
        spanner_internal::SessionPoolFriendForTest::AsyncRefreshSession(
            session_pool, cq, stub, session_name)
            .then([session_name](
                      future<StatusOr<google::spanner::v1::ResultSet>> f) {
              auto result = f.get();
              EXPECT_STATUS_OK(result);
              return result.ok();
            }));
  }
  for (auto& ag : async_get) {
    auto matched = ag.get();
    EXPECT_TRUE(matched);
  }

  std::vector<future<Status>> async_delete;
  for (auto const& s : create_response->session()) {
    auto const& session_name = s.name();
    async_delete.push_back(SessionPoolFriendForTest::AsyncDeleteSession(
        session_pool, cq, stub, session_name));
  }
  for (auto& ad : async_delete) {
    auto status = ad.get();
    EXPECT_STATUS_OK(status);
  }

  cq.Shutdown();
  t.join();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
