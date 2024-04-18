// Copyright 2022 Google LLC
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

#include "google/cloud/pubsub/internal/extend_leases_with_retry.h"
#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/testing/mock_batch_callback.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::pubsub_testing::MockSubscriberStub;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::pubsub::v1::ModifyAckDeadlineRequest;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Return;

Status MakeTransient() { return Status(StatusCode::kUnavailable, "try-again"); }
Status MakeStatusWithDetails(
    std::vector<std::pair<std::string, std::string>> const& details) {
  return Status(StatusCode::kUnknown, "uh?",
                ErrorInfo("test-reason", "test-domain",
                          {details.begin(), details.end()}));
}

TEST(ExtendLeasesWithRetry, Success) {
  auto mock = std::make_shared<MockSubscriberStub>();
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _, _,
                           Property(&ModifyAckDeadlineRequest::ack_ids,
                                    ElementsAre("test-001", "test-002"))))
        .WillOnce(Return(ByMove(make_ready_future(MakeTransient()))));
    EXPECT_CALL(*mock_cq, MakeRelativeTimer)
        .WillOnce(Return(ByMove(make_ready_future(
            make_status_or(std::chrono::system_clock::now())))));
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _, _,
                           Property(&ModifyAckDeadlineRequest::ack_ids,
                                    ElementsAre("test-001", "test-002"))))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  }

  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();

  ModifyAckDeadlineRequest request;
  request.add_ack_ids("test-001");
  request.add_ack_ids("test-002");

  auto result =
      ExtendLeasesWithRetry(mock, CompletionQueue(mock_cq), request,
                            mock_batch_callback, /*enable_otel=*/false);

  EXPECT_STATUS_OK(result.get());
}

TEST(ExtendLeasesWithRetry, SuccessWithPartials) {
  auto mock = std::make_shared<MockSubscriberStub>();
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _, _,
                           Property(&ModifyAckDeadlineRequest::ack_ids,
                                    ElementsAre("test-001", "test-002",
                                                "test-003", "test-004"))))
        .WillOnce(Return(ByMove(make_ready_future(MakeStatusWithDetails({
            {"test-001", "TRANSIENT_FAILURE_1"},
            {"test-002", "TRANSIENT_FAILURE_2"},
            {"test-003", "PERMANENT_BADNESS"},
        })))));
    EXPECT_CALL(*mock_cq, MakeRelativeTimer)
        .WillOnce(Return(ByMove(make_ready_future(
            make_status_or(std::chrono::system_clock::now())))));
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _, _,
                           Property(&ModifyAckDeadlineRequest::ack_ids,
                                    ElementsAre("test-001", "test-002"))))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  }

  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();

  ModifyAckDeadlineRequest request;
  request.add_ack_ids("test-001");
  request.add_ack_ids("test-002");
  request.add_ack_ids("test-003");
  request.add_ack_ids("test-004");
  auto result =
      ExtendLeasesWithRetry(mock, CompletionQueue(mock_cq), request,
                            mock_batch_callback, /*enable_otel=*/false);
  EXPECT_STATUS_OK(result.get());
}

TEST(ExtendLeasesWithRetry, FailurePermanentError) {
  auto mock = std::make_shared<MockSubscriberStub>();
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();

  EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                         _, _, _,
                         Property(&ModifyAckDeadlineRequest::ack_ids,
                                  ElementsAre("test-001", "test-002"))))
      .WillOnce(Return(ByMove(make_ready_future(MakeStatusWithDetails({})))));
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();

  ModifyAckDeadlineRequest request;
  request.add_ack_ids("test-001");
  request.add_ack_ids("test-002");
  auto result =
      ExtendLeasesWithRetry(mock, CompletionQueue(mock_cq), request,
                            mock_batch_callback, /*enable_otel=*/false);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kUnknown));
}

TEST(ExtendLeasesWithRetry, FailureTooManyTransients) {
  auto mock = std::make_shared<MockSubscriberStub>();
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock,
                AsyncModifyAckDeadline(
                    _, _, _,
                    Property(&ModifyAckDeadlineRequest::ack_ids,
                             ElementsAre("test-001", "test-002", "test-003"))))
        .WillOnce(Return(ByMove(make_ready_future(MakeStatusWithDetails({
            {"test-001", "TRANSIENT_FAILURE_1"},
            {"test-002", "TRANSIENT_FAILURE_2"},
            {"test-003", "PERMANENT_ERROR_INVALID_BLAH"},
        })))));
    for (int i = 0; i != 2; ++i) {
      EXPECT_CALL(*mock_cq, MakeRelativeTimer)
          .WillOnce(Return(ByMove(make_ready_future(
              make_status_or(std::chrono::system_clock::now())))));
      EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                             _, _, _,
                             Property(&ModifyAckDeadlineRequest::ack_ids,
                                      ElementsAre("test-001", "test-002"))))
          .WillOnce(Return(ByMove(make_ready_future(MakeStatusWithDetails({
              {"test-001", "TRANSIENT_FAILURE_1"},
              {"test-002", "TRANSIENT_FAILURE_2"},
          })))));
    }
  }
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();

  google::cloud::testing_util::ScopedLog log;
  ModifyAckDeadlineRequest request;
  request.add_ack_ids("test-001");
  request.add_ack_ids("test-002");
  request.add_ack_ids("test-003");
  auto result =
      ExtendLeasesWithRetry(mock, CompletionQueue(mock_cq), request,
                            mock_batch_callback, /*enable_otel=*/false);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kUnknown));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ack_id=test-001")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ack_id=test-002")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ack_id=test-003")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

TEST(ExtendLeasesWithRetry, SuccessWithOtelEnabled) {
  auto mock = std::make_shared<MockSubscriberStub>();
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _, _,
                           Property(&ModifyAckDeadlineRequest::ack_ids,
                                    ElementsAre("test-001", "test-002"))))
        .WillOnce(Return(ByMove(make_ready_future(MakeTransient()))));
    EXPECT_CALL(*mock_cq, MakeRelativeTimer)
        .WillOnce(Return(ByMove(make_ready_future(
            make_status_or(std::chrono::system_clock::now())))));
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _, _,
                           Property(&ModifyAckDeadlineRequest::ack_ids,
                                    ElementsAre("test-001", "test-002"))))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  }

  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartModackSpan).Times(2);
  EXPECT_CALL(*mock_batch_callback, EndModackSpan).Times(2);
  EXPECT_CALL(*mock_batch_callback, ModackEnd("test-001")).Times(2);
  EXPECT_CALL(*mock_batch_callback, ModackEnd("test-002")).Times(2);

  ModifyAckDeadlineRequest request;
  request.add_ack_ids("test-001");
  request.add_ack_ids("test-002");

  auto result =
      ExtendLeasesWithRetry(mock, CompletionQueue(mock_cq), request,
                            mock_batch_callback, /*enable_otel=*/true);

  EXPECT_STATUS_OK(result.get());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
