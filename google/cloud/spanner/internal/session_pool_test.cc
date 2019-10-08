// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::ByMove;
using ::testing::Return;

class MockSessionManager : public SessionManager {
 public:
  MOCK_METHOD1(CreateSessions,
               StatusOr<std::vector<std::unique_ptr<Session>>>(int));
};

// Helper to build a vector of Sessions.
std::vector<std::unique_ptr<Session>> MakeSessions(
    std::vector<std::string> session_names) {
  std::vector<std::unique_ptr<Session>> ret;
  ret.reserve(session_names.size());
  for (auto& name : session_names) {
    ret.push_back(
        google::cloud::internal::make_unique<Session>(std::move(name)));
  }
  return ret;
}

TEST(SessionPool, Allocate) {
  auto mock = google::cloud::internal::make_unique<MockSessionManager>();
  EXPECT_CALL(*mock, CreateSessions(1))
      .WillOnce(Return(ByMove(MakeSessions({"session1"}))));

  SessionPool pool(mock.get());
  auto session = pool.Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST(SessionPool, CreateError) {
  auto mock = google::cloud::internal::make_unique<MockSessionManager>();
  EXPECT_CALL(*mock, CreateSessions(1))
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "some failure"))));

  SessionPool pool(mock.get());
  auto session = pool.Allocate();
  EXPECT_EQ(session.status().code(), StatusCode::kInternal);
  EXPECT_EQ(session.status().message(), "some failure");
}

TEST(SessionPool, ReuseSession) {
  auto mock = google::cloud::internal::make_unique<MockSessionManager>();
  EXPECT_CALL(*mock, CreateSessions(1))
      .WillOnce(Return(ByMove(MakeSessions({"session1"}))));

  SessionPool pool(mock.get());
  auto session = pool.Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
  pool.Release(*std::move(session));

  auto session2 = pool.Allocate();
  ASSERT_STATUS_OK(session2);
  EXPECT_EQ((*session2)->session_name(), "session1");
  pool.Release(*std::move(session2));
}

TEST(SessionPool, Lifo) {
  auto mock = google::cloud::internal::make_unique<MockSessionManager>();
  EXPECT_CALL(*mock, CreateSessions(1))
      .WillOnce(Return(ByMove(MakeSessions({"session1"}))))
      .WillOnce(Return(ByMove(MakeSessions({"session2"}))));

  SessionPool pool(mock.get());
  auto session = pool.Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");

  auto session2 = pool.Allocate();
  ASSERT_STATUS_OK(session2);
  EXPECT_EQ((*session2)->session_name(), "session2");

  pool.Release(*std::move(session));
  pool.Release(*std::move(session2));

  // The pool is Last-In-First-Out (LIFO), so we expect to get the sessions
  // back in the reverse order they were released.
  auto session3 = pool.Allocate();
  ASSERT_STATUS_OK(session3);
  EXPECT_EQ((*session3)->session_name(), "session2");

  auto session4 = pool.Allocate();
  ASSERT_STATUS_OK(session4);
  EXPECT_EQ((*session4)->session_name(), "session1");
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
