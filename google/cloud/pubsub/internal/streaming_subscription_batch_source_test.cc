// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads;
using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Unused;

class FakeStream {
 public:
  FakeStream(bool start_result, bool write_result, Status finish)
      : start_result_(start_result),
        write_result_(write_result),
        finish_(std::move(finish)) {}

  promise<bool> WaitForAction() {
    GCP_LOG(DEBUG) << __func__ << "()";
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return !actions_.empty(); });
    auto action = std::move(actions_.front());
    actions_.pop_front();
    return action;
  }

  std::unique_ptr<pubsub_testing::MockAsyncPullStream> MakeWriteFailureStream(
      google::cloud::CompletionQueue& cq, std::unique_ptr<grpc::ClientContext>,
      google::pubsub::v1::StreamingPullRequest const&) {
    using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
    using us = std::chrono::microseconds;
    auto start_response = [&cq](bool v) {
      return [cq, v]() mutable {
        return cq.MakeRelativeTimer(us(10)).then(
            [v](TimerFuture) { return v; });
      };
    };
    auto initial_write_response = [&cq](bool v) {
      return [cq, v](google::pubsub::v1::StreamingPullRequest const& request,
                     grpc::WriteOptions const&) mutable {
        EXPECT_FALSE(request.client_id().empty());
        EXPECT_FALSE(request.subscription().empty());
        return cq.MakeRelativeTimer(us(10)).then(
            [v](TimerFuture) { return v; });
      };
    };
    auto read_response = [this] {
      return AddAction("Read").then([](future<bool> g) mutable {
        auto ok = g.get();
        using Response = google::pubsub::v1::StreamingPullResponse;
        if (!ok) return absl::optional<Response>{};
        return absl::make_optional(Response{});
      });
    };
    auto finish_response = [&cq](Status const& status) {
      return [cq, status]() mutable {
        return cq.MakeRelativeTimer(us(10)).then(
            [status](TimerFuture) { return status; });
      };
    };

    auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
    EXPECT_CALL(*stream, Start).WillOnce(start_response(start_result_));
    if (start_result_) {
      EXPECT_CALL(*stream, Write)
          .WillOnce(initial_write_response(write_result_));
      if (write_result_) {
        EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
      }
    }

    EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
    EXPECT_CALL(*stream, Finish).WillOnce(finish_response(finish_));

    return stream;
  }

  future<bool> AddAction(char const* caller) {
    GCP_LOG(DEBUG) << __func__ << "(" << caller << ")";
    std::unique_lock<std::mutex> lk(mu_);
    actions_.emplace_back(promise<bool>{});
    auto f = actions_.back().get_future();
    lk.unlock();
    cv_.notify_one();
    return f;
  }

 private:
  bool const start_result_;
  bool const write_result_;
  Status finish_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<promise<bool>> actions_;
};

pubsub::SubscriptionOptions TestSubscriptionOptions() {
  return pubsub::SubscriptionOptions()
      .set_max_outstanding_messages(100)
      .set_max_outstanding_bytes(100 * 1024 * 1024L)
      .set_max_deadline_time(std::chrono::seconds(300));
}

TEST(StreamingSubscriptionBatchSourceTest, Start) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream success_stream(true, true, Status{});

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return success_stream.MakeWriteFailureStream(cq, std::move(context),
                                                     request);
      });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  success_stream.WaitForAction().set_value(true);
  auto last = success_stream.WaitForAction();
  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last.set_value(false);

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

TEST(StreamingSubscriptionBatchSourceTest, StartWithRetry) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const transient = Status{StatusCode::kUnavailable, "try-again"};
  FakeStream start_failure(false, false, transient);
  FakeStream write_failure(true, false, transient);
  FakeStream success_stream(true, true, Status{});

  auto make_async_pull_mock = [](FakeStream& fake) {
    return [&fake](google::cloud::CompletionQueue& cq,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::pubsub::v1::StreamingPullRequest const& request) {
      return fake.MakeWriteFailureStream(cq, std::move(context), request);
    };
  };

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce(make_async_pull_mock(start_failure))
      .WillOnce(make_async_pull_mock(write_failure))
      .WillOnce(make_async_pull_mock(success_stream));

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  success_stream.WaitForAction().set_value(true);
  auto last = success_stream.WaitForAction();
  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last.set_value(false);

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

TEST(StreamingSubscriptionBatchSourceTest, StartFailure) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const transient = Status{StatusCode::kUnavailable, "try-again"};
  FakeStream write_failure(true, false, transient);

  auto make_async_pull_mock = [](FakeStream& fake) {
    return [&fake](google::cloud::CompletionQueue& cq,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::pubsub::v1::StreamingPullRequest const& request) {
      return fake.MakeWriteFailureStream(cq, std::move(context), request);
    };
  };

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(2))
      .WillRepeatedly(make_async_pull_mock(write_failure));

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  promise<Status> p;
  uut->Start([&](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    p.set_value(std::move(r).status());
  });
  auto status = p.get_future().get();
  EXPECT_THAT(status,
              StatusIs(transient.code(), HasSubstr(transient.message())));
  uut->Shutdown();

  EXPECT_EQ(done.get(), status);
}

TEST(StreamingSubscriptionBatchSourceTest, StartUnexpected) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(1)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::StreamingPullRequest const&) {
        return std::unique_ptr<SubscriberStub::AsyncPullStream>{};
      });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  promise<Status> p;
  uut->Start([&](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    p.set_value(std::move(r).status());
  });
  auto status = p.get_future().get();
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown));
  uut->Shutdown();

  EXPECT_EQ(done.get(), status);
}

TEST(StreamingSubscriptionBatchSourceTest, StartSucceedsWithNull) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  // Fake a request that "succeeds" on a null stream.
  FakeStream fake_stream(true, false, {});

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return fake_stream.MakeWriteFailureStream(cq, std::move(context),
                                                  request);
      });

  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  promise<void> received_call;
  EXPECT_CALL(callback, Call(StatusIs(StatusCode::kUnknown)))
      .WillOnce(
          [&received_call](CallbackArg const&) { received_call.set_value(); });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  // Wait until we receive the callback to shutdown, otherwise we might race by
  // shutting down before the status is set.
  received_call.get_future().get();

  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  EXPECT_THAT(done.get(), StatusIs(StatusCode::kUnknown));
}

TEST(StreamingSubscriptionBatchSourceTest, StartSucceedsAfterShutdown) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  promise<bool> start;
  promise<bool> initial_write;
  promise<Status> finish;
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::StreamingPullRequest const&) {
        auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&start] {
          return start.get_future();
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce([&initial_write](::testing::Unused, ::testing::Unused) {
              return initial_write.get_future();
            });

        EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
        EXPECT_CALL(*stream, Finish).WillOnce([&finish] {
          return finish.get_future();
        });

        return stream;
      });

  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call).Times(0);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  start.set_value(true);
  shutdown->MarkAsShutdown("test", {});
  initial_write.set_value(true);
  finish.set_value(Status{StatusCode::kCancelled, "cancelled"});

  EXPECT_EQ(Status{}, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, AckMany) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream success_stream(true, true, Status{});

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        auto stream = success_stream.MakeWriteFailureStream(
            cq, std::move(context), request);
        using Request = google::pubsub::v1::StreamingPullRequest;
        // Add expectations for Write() calls with empty subscriptions, only
        // the first call has a non-empty value and it is already set.
        EXPECT_CALL(*stream,
                    Write(Property(&Request::subscription, std::string{}), _))
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.ack_ids(), ElementsAre("fake-001"));
                  EXPECT_THAT(request.modify_deadline_ack_ids(), IsEmpty());
                  EXPECT_THAT(request.modify_deadline_seconds(), IsEmpty());
                  EXPECT_THAT(request.client_id(), IsEmpty());
                  EXPECT_THAT(request.subscription(), IsEmpty());
                  return success_stream.AddAction("Write");
                })
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.ack_ids(), ElementsAre("fake-002"));
                  EXPECT_THAT(request.modify_deadline_ack_ids(),
                              ElementsAre("fake-003"));
                  EXPECT_THAT(request.modify_deadline_seconds(),
                              ElementsAre(0));
                  EXPECT_THAT(request.client_id(), IsEmpty());
                  EXPECT_THAT(request.subscription(), IsEmpty());
                  return success_stream.AddAction("Write");
                })
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.modify_deadline_ack_ids(),
                              ElementsAre("fake-004", "fake-005"));
                  EXPECT_THAT(request.modify_deadline_seconds(),
                              ElementsAre(0, 0));
                  EXPECT_THAT(request.ack_ids(), IsEmpty());
                  EXPECT_THAT(request.client_id(), IsEmpty());
                  EXPECT_THAT(request.subscription(), IsEmpty());
                  return success_stream.AddAction("Write");
                })
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.modify_deadline_ack_ids(),
                              ElementsAre("fake-006"));
                  EXPECT_THAT(request.modify_deadline_seconds(),
                              ElementsAre(10));
                  EXPECT_THAT(request.ack_ids(), IsEmpty());
                  EXPECT_THAT(request.client_id(), IsEmpty());
                  EXPECT_THAT(request.subscription(), IsEmpty());
                  return success_stream.AddAction("Write");
                });
        return stream;
      });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  success_stream.WaitForAction().set_value(true);
  auto last_read = success_stream.WaitForAction();

  uut->AckMessage("fake-001", 0);
  uut->AckMessage("fake-002", 0);
  uut->NackMessage("fake-003", 0);
  // Flush the first AckMessage()
  success_stream.WaitForAction().set_value(true);
  // Flush the compiled AckMessage() + NackMessage()
  success_stream.WaitForAction().set_value(true);

  uut->BulkNack({"fake-004", "fake-005"}, 0);
  success_stream.WaitForAction().set_value(true);

  uut->ExtendLeases({"fake-006"}, std::chrono::seconds(10));
  success_stream.WaitForAction().set_value(true);

  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last_read.set_value(false);

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

TEST(StreamingSubscriptionBatchSourceTest, ReadErrorWaitsForWrite) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const expected_status = Status{StatusCode::kNotFound, "gone"};
  FakeStream fake_stream(true, true, expected_status);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        auto stream =
            fake_stream.MakeWriteFailureStream(cq, std::move(context), request);
        using Request = google::pubsub::v1::StreamingPullRequest;
        // Add expectations for Write() calls with empty subscriptions, only
        // the first call has a non-empty value and it is already set.
        EXPECT_CALL(*stream,
                    Write(Property(&Request::subscription, std::string{}), _))
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.ack_ids(), ElementsAre("fake-001"));
                  return fake_stream.AddAction("Write");
                });
        return stream;
      });
  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(StatusIs(StatusCode::kOk))).Times(1);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  fake_stream.WaitForAction().set_value(true);

  auto pending_read = fake_stream.WaitForAction();
  uut->AckMessage("fake-001", 0);
  auto pending_write = fake_stream.WaitForAction();

  pending_read.set_value(false);
  shutdown->MarkAsShutdown("test", expected_status);
  uut->Shutdown();

  pending_write.set_value(true);

  EXPECT_EQ(expected_status, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, WriteErrorWaitsForRead) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const expected_status = Status{StatusCode::kNotFound, "gone"};
  FakeStream fake_stream(true, true, expected_status);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        auto stream =
            fake_stream.MakeWriteFailureStream(cq, std::move(context), request);
        using Request = google::pubsub::v1::StreamingPullRequest;
        // Add expectations for Write() calls with empty subscriptions, only
        // the first call has a non-empty value and it is already set.
        EXPECT_CALL(*stream,
                    Write(Property(&Request::subscription, std::string{}), _))
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.ack_ids(), ElementsAre("fake-001"));
                  return fake_stream.AddAction("Write");
                });
        return stream;
      });
  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(StatusIs(StatusCode::kOk))).Times(1);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  fake_stream.WaitForAction().set_value(true);

  auto pending_read = fake_stream.WaitForAction();
  uut->AckMessage("fake-001", 0);
  auto pending_write = fake_stream.WaitForAction();

  shutdown->MarkAsShutdown("test", expected_status);
  uut->Shutdown();

  pending_write.set_value(false);
  pending_read.set_value(false);

  EXPECT_EQ(expected_status, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, ShutdownWithPendingRead) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const expected_status = Status{};
  FakeStream fake_stream(true, true, expected_status);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return fake_stream.MakeWriteFailureStream(cq, std::move(context),
                                                  request);
      });
  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(StatusIs(StatusCode::kOk))).Times(0);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  auto pending_read = fake_stream.WaitForAction();

  uut->Shutdown();
  shutdown->MarkAsShutdown("test", expected_status);

  pending_read.set_value(true);
  EXPECT_EQ(expected_status, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, Resume) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  std::string const client_id = "fake-client-id";
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream stream0(true, true, Status{StatusCode::kUnavailable, "try-again"});
  FakeStream stream1(true, true, Status{});

  auto make_mock = [](FakeStream& fake_stream) {
    return [&fake_stream](
               google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::pubsub::v1::StreamingPullRequest const& request) {
      auto stream =
          fake_stream.MakeWriteFailureStream(cq, std::move(context), request);
      using Request = google::pubsub::v1::StreamingPullRequest;
      // Add expectations for Write() calls with empty subscriptions, only
      // the first call has a non-empty value and it is already set.
      EXPECT_CALL(*stream,
                  Write(Property(&Request::subscription, std::string{}), _))
          .WillRepeatedly([&](google::pubsub::v1::StreamingPullRequest const&,
                              grpc::WriteOptions const&) {
            return make_ready_future(true);
          });
      return stream;
    };
  };

  // We abuse this test to verify logging works as expected too
  auto capture = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto const id = google::cloud::LogSink::Instance().AddBackend(capture);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce(make_mock(stream0))
      .WillOnce(make_mock(stream1));

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = std::make_shared<StreamingSubscriptionBatchSource>(
      background.cq(), shutdown, mock, subscription.FullName(), client_id,
      TestSubscriptionOptions(), TestRetryPolicy(), TestBackoffPolicy());

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  auto read0 = stream0.WaitForAction();
  EXPECT_THAT(capture->ClearLogLines(), Contains(HasSubstr("OnStreamStart")));
  read0.set_value(false);  // Read()

  auto read1 = stream1.WaitForAction();
  EXPECT_THAT(capture->ClearLogLines(), Contains(HasSubstr("OnStreamStart")));
  read1.set_value(true);                // Read()
  auto last = stream1.WaitForAction();  // Read()
  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last.set_value(false);
  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

TEST(StreamingSubscriptionBatchSourceTest, StateOStream) {
  auto as_string = [](StreamingSubscriptionBatchSource::StreamState s) {
    std::ostringstream os;
    os << s;
    return std::move(os).str();
  };
  using StreamState = StreamingSubscriptionBatchSource::StreamState;
  EXPECT_EQ("kNull", as_string(StreamState::kNull));
  EXPECT_EQ("kActive", as_string(StreamState::kActive));
  EXPECT_EQ("kDisconnecting", as_string(StreamState::kDisconnecting));
  EXPECT_EQ("kFinishing", as_string(StreamState::kFinishing));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
