// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads;
using ::google::cloud::internal::RunAsyncBase;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Return;
using ::testing::Unused;

using AckRequest = ::google::pubsub::v1::AcknowledgeRequest;
using ModifyRequest = ::google::pubsub::v1::ModifyAckDeadlineRequest;

class FakeStream {
 public:
  explicit FakeStream(Status finish) : finish_(std::move(finish)) {}

  promise<bool> WaitForAction() {
    auto p = async_.PopFrontWithName();
    GCP_LOG(DEBUG) << __func__ << "(" << p.second << ")";
    return std::move(p.first);
  }

  std::unique_ptr<pubsub_testing::MockAsyncPullStream> MakeWriteFailureStream(
      google::cloud::CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::pubsub::v1::StreamingPullRequest const&) {
    auto start_response = [this] {
      return AddAction("Start").then([](future<bool> g) { return g.get(); });
    };
    auto write_response = [this](
                              google::pubsub::v1::StreamingPullRequest const&,
                              grpc::WriteOptions const&) {
      return AddAction("Write").then([](future<bool> g) { return g.get(); });
    };
    auto read_response = [this] {
      return AddAction("Read").then([](future<bool> g) {
        auto ok = g.get();
        using Response = ::google::pubsub::v1::StreamingPullResponse;
        if (!ok) return absl::optional<Response>{};
        return absl::make_optional(Response{});
      });
    };
    auto finish_response = [this] {
      auto s = finish_;
      return AddAction("Finish").then(
          [s](future<bool>) mutable { return std::move(s); });
    };

    auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
    EXPECT_CALL(*stream, Start).WillOnce(start_response);
    EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
    EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
    EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
    EXPECT_CALL(*stream, Finish)
        .Times(AtMost(1))
        .WillRepeatedly(finish_response);

    return stream;
  }

  future<bool> AddAction(char const* caller) {
    GCP_LOG(DEBUG) << __func__ << "(" << caller << ")";
    return async_.PushBack(caller);
  }

 private:
  Status finish_;
  AsyncSequencer<bool> async_;
};

std::shared_ptr<StreamingSubscriptionBatchSource> MakeTestBatchSource(
    CompletionQueue cq, std::shared_ptr<SessionShutdownManager> shutdown,
    std::shared_ptr<SubscriberStub> mock) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto opts = DefaultSubscriberOptions(pubsub_testing::MakeTestOptions(
      Options{}
          .set<pubsub::MaxOutstandingMessagesOption>(100)
          .set<pubsub::MaxOutstandingBytesOption>(100 * 1024 * 1024L)
          .set<pubsub::MaxHoldTimeOption>(std::chrono::seconds(300))));
  return std::make_shared<StreamingSubscriptionBatchSource>(
      std::move(cq), std::move(shutdown), std::move(mock),
      std::move(subscription).FullName(), "test-client-id", std::move(opts),
      AckBatchingConfig(1, std::chrono::milliseconds(10)));
}

TEST(StreamingSubscriptionBatchSourceTest, Start) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream success_stream(Status{});

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return success_stream.MakeWriteFailureStream(cq, std::move(context),
                                                     request);
      });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  success_stream.WaitForAction().set_value(true);  // Start()
  success_stream.WaitForAction().set_value(true);  // Write()
  success_stream.WaitForAction().set_value(true);  // Read()
  auto last = success_stream.WaitForAction();      // Read()
  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last.set_value(false);
  success_stream.WaitForAction().set_value(true);  // Finish()

  EXPECT_THAT(done.get(), IsOk());
}

TEST(StreamingSubscriptionBatchSourceTest, StartWithRetry) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const transient = Status{StatusCode::kUnavailable, "try-again"};
  FakeStream start_failure(transient);
  FakeStream write_failure(transient);
  FakeStream success_stream(Status{});

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
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});

  start_failure.WaitForAction().set_value(false);  // Start()
  start_failure.WaitForAction().set_value(true);   // Finish()

  write_failure.WaitForAction().set_value(true);   // Start()
  write_failure.WaitForAction().set_value(false);  // Write()
  write_failure.WaitForAction().set_value(true);   // Finish()

  success_stream.WaitForAction().set_value(true);  // Start()
  success_stream.WaitForAction().set_value(true);  // Write()
  success_stream.WaitForAction().set_value(true);  // Read()
  auto last = success_stream.WaitForAction();
  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last.set_value(false);
  success_stream.WaitForAction().set_value(true);  // Finish()

  EXPECT_THAT(done.get(), IsOk());
}

TEST(StreamingSubscriptionBatchSourceTest, StartTooManyTransientFailures) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const transient = Status{StatusCode::kUnavailable, "try-again"};

  auto async_pull_mock = [transient](
                             google::cloud::CompletionQueue& cq,
                             std::unique_ptr<grpc::ClientContext>,
                             google::pubsub::v1::StreamingPullRequest const&) {
    using us = std::chrono::microseconds;
    using F = future<StatusOr<std::chrono::system_clock::time_point>>;
    using Response = ::google::pubsub::v1::StreamingPullResponse;
    auto start_response = [cq]() mutable {
      return cq.MakeRelativeTimer(us(10)).then([](F) { return true; });
    };
    auto write_response = [cq](google::pubsub::v1::StreamingPullRequest const&,
                               grpc::WriteOptions const&) mutable {
      return cq.MakeRelativeTimer(us(10)).then([](F) { return true; });
    };
    auto read_response = [cq]() mutable {
      return cq.MakeRelativeTimer(us(10)).then(
          [](F) { return absl::optional<Response>{}; });
    };
    auto finish_response = [cq, transient]() mutable {
      return cq.MakeRelativeTimer(us(10)).then(
          // NOLINTNEXTLINE(performance-no-automatic-move)
          [transient](F) { return transient; });
    };

    auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
    EXPECT_CALL(*stream, Start).WillOnce(start_response);
    EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
    EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
    EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
    EXPECT_CALL(*stream, Finish).WillOnce(finish_response);

    return stream;
  };

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(2))
      .WillRepeatedly(async_pull_mock);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  promise<Status> p;
  uut->Start([&](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    p.set_value(std::move(r).status());
  });
  auto const status = p.get_future().get();
  EXPECT_THAT(status,
              StatusIs(transient.code(), HasSubstr(transient.message())));
  uut->Shutdown();

  EXPECT_EQ(done.get(), status);
}

TEST(StreamingSubscriptionBatchSourceTest, StartPermanentFailure) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const transient = Status{StatusCode::kPermissionDenied, "uh-oh"};

  auto async_pull_mock = [transient](
                             google::cloud::CompletionQueue& cq,
                             std::unique_ptr<grpc::ClientContext>,
                             google::pubsub::v1::StreamingPullRequest const&) {
    using us = std::chrono::microseconds;
    using F = future<StatusOr<std::chrono::system_clock::time_point>>;
    using Response = ::google::pubsub::v1::StreamingPullResponse;
    auto start_response = [cq]() mutable {
      return cq.MakeRelativeTimer(us(10)).then([](F) { return true; });
    };
    auto write_response = [cq](google::pubsub::v1::StreamingPullRequest const&,
                               grpc::WriteOptions const&) mutable {
      return cq.MakeRelativeTimer(us(10)).then([](F) { return true; });
    };
    auto read_response = [cq]() mutable {
      return cq.MakeRelativeTimer(us(10)).then(
          [](F) { return absl::optional<Response>{}; });
    };
    auto finish_response = [cq, transient]() mutable {
      return cq.MakeRelativeTimer(us(10)).then(
          // NOLINTNEXTLINE(performance-no-automatic-move)
          [transient](F) { return transient; });
    };

    auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
    EXPECT_CALL(*stream, Start).WillOnce(start_response);
    EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
    EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
    EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
    EXPECT_CALL(*stream, Finish).WillOnce(finish_response);

    return stream;
  };

  EXPECT_CALL(*mock, AsyncStreamingPull).WillOnce(async_pull_mock);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  promise<Status> p;
  uut->Start([&](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    p.set_value(std::move(r).status());
  });
  auto const status = p.get_future().get();
  EXPECT_THAT(status,
              StatusIs(transient.code(), HasSubstr(transient.message())));
  uut->Shutdown();

  EXPECT_EQ(done.get(), status);
}

TEST(StreamingSubscriptionBatchSourceTest, StartUnexpected) {
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
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

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

TEST(StreamingSubscriptionBatchSourceTest, StartSucceedsAfterStartAndShutdown) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream success_stream(Status{StatusCode::kCancelled, "cancelled"});

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return success_stream.MakeWriteFailureStream(cq, std::move(context),
                                                     request);
      });

  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call).Times(0);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  success_stream.WaitForAction().set_value(true);  // Start()
  shutdown->MarkAsShutdown("test", {});
  success_stream.WaitForAction().set_value(true);  // Start() / retry

  EXPECT_EQ(Status{}, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, StartSucceedsAfterWriteAndShutdown) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream success_stream(Status{StatusCode::kCancelled, "cancelled"});

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return success_stream.MakeWriteFailureStream(cq, std::move(context),
                                                     request);
      });

  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call).Times(0);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  success_stream.WaitForAction().set_value(true);  // Start()
  success_stream.WaitForAction().set_value(true);  // Write()
  shutdown->MarkAsShutdown("test", {});
  success_stream.WaitForAction().set_value(true);  // Read()
  success_stream.WaitForAction().set_value(true);  // Finish()

  EXPECT_EQ(Status{}, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, ResumeAfterFirstRead) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto make_async_pull_mock = [](int start, int count) {
    return [start, count](google::cloud::CompletionQueue& cq,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::StreamingPullRequest const&) {
      using us = std::chrono::microseconds;
      using F = future<StatusOr<std::chrono::system_clock::time_point>>;
      using Response = ::google::pubsub::v1::StreamingPullResponse;
      auto start_response = [cq]() mutable {
        return cq.MakeRelativeTimer(us(10)).then([](F) { return true; });
      };
      auto write_response = [cq](
                                google::pubsub::v1::StreamingPullRequest const&,
                                grpc::WriteOptions const&) mutable {
        return cq.MakeRelativeTimer(us(10)).then([](F) { return true; });
      };
      auto read_success = [cq, start, count]() mutable {
        return cq.MakeRelativeTimer(us(10)).then([start, count](F) {
          Response response;
          for (int i = 0; i != count; ++i) {
            response.add_received_messages()->set_ack_id(
                "ack-" + std::to_string(start + i));
          }
          return absl::make_optional(std::move(response));
        });
      };
      auto read_failure = [cq]() mutable {
        return cq.MakeRelativeTimer(us(10)).then(
            [](F) { return absl::optional<Response>{}; });
      };
      auto finish_response = [cq]() mutable {
        return cq.MakeRelativeTimer(us(10)).then([](F) mutable {
          return Status{StatusCode::kUnavailable, "try-again"};
        });
      };

      auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
      EXPECT_CALL(*stream, Start).WillOnce(start_response);
      EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
      EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
      EXPECT_CALL(*stream, Read).WillOnce(read_success).WillOnce(read_failure);
      EXPECT_CALL(*stream, Finish).WillOnce(finish_response);

      return stream;
    };
  };

  promise<void> ready;
  promise<void> wait;
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce(make_async_pull_mock(0, 3))
      .WillOnce(make_async_pull_mock(3, 2))
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::StreamingPullRequest const&) {
        ready.set_value();
        wait.get_future().wait();
        return nullptr;
      });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  std::vector<std::string> ids;
  uut->Start([&](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    if (!r) return;
    for (auto const& m : r->received_messages()) ids.push_back(m.ack_id());
  });
  ready.get_future().wait();
  shutdown->MarkAsShutdown("test", {});
  wait.set_value();
  EXPECT_EQ(done.get(), Status{});
  EXPECT_THAT(ids, ElementsAre("ack-0", "ack-1", "ack-2", "ack-3", "ack-4"));
}

future<Status> OnAck(google::cloud::CompletionQueue&,
                     std::unique_ptr<grpc::ClientContext>, AckRequest const&) {
  return make_ready_future(Status{});
}

future<Status> OnModify(google::cloud::CompletionQueue&,
                        std::unique_ptr<grpc::ClientContext>,
                        ModifyRequest const&) {
  return make_ready_future(Status{});
}

TEST(StreamingSubscriptionBatchSourceTest, AckMany) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  FakeStream success_stream(Status{});
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncStreamingPull)
        .WillOnce([&](google::cloud::CompletionQueue& cq,
                      std::unique_ptr<grpc::ClientContext> context,
                      google::pubsub::v1::StreamingPullRequest const& request) {
          auto stream = success_stream.MakeWriteFailureStream(
              cq, std::move(context), request);
          using Request = ::google::pubsub::v1::StreamingPullRequest;
          // Add expectations for Write() calls with empty subscriptions, only
          // the first call has a non-empty value, and it is already set.
          EXPECT_CALL(*stream,
                      Write(Property(&Request::subscription, std::string{}), _))
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
    EXPECT_CALL(*mock, AsyncAcknowledge(_, _,
                                        Property(&AckRequest::ack_ids,
                                                 ElementsAre("fake-001"))))
        .WillOnce(OnAck);
    EXPECT_CALL(*mock, AsyncAcknowledge(_, _,
                                        Property(&AckRequest::ack_ids,
                                                 ElementsAre("fake-002"))))
        .WillOnce(OnAck);
    EXPECT_CALL(
        *mock,
        AsyncModifyAckDeadline(
            _, _, Property(&ModifyRequest::ack_ids, ElementsAre("fake-003"))))
        .WillOnce(OnModify);
    EXPECT_CALL(*mock, AsyncModifyAckDeadline(
                           _, _,
                           Property(&ModifyRequest::ack_ids,
                                    ElementsAre("fake-004", "fake-005"))))
        .WillOnce(OnModify);
  }

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  success_stream.WaitForAction().set_value(true);  // Start()
  success_stream.WaitForAction().set_value(true);  // Write()
  success_stream.WaitForAction().set_value(true);  // Read()
  auto last_read = success_stream.WaitForAction();

  // None of these trigger events in the stream, they satisfy the expectations
  // set on `*mock`:
  uut->AckMessage("fake-001");
  uut->AckMessage("fake-002");
  uut->NackMessage("fake-003");
  uut->BulkNack({"fake-004", "fake-005"});

  uut->ExtendLeases({"fake-006"}, std::chrono::seconds(10));
  success_stream.WaitForAction().set_value(true);  // Write()

  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  last_read.set_value(false);                      // Read()
  success_stream.WaitForAction().set_value(true);  // Finish()

  EXPECT_THAT(done.get(), IsOk());
}

TEST(StreamingSubscriptionBatchSourceTest, ReadErrorWaitsForWrite) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const expected_status = Status{StatusCode::kNotFound, "gone"};
  FakeStream fake_stream(expected_status);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        auto stream =
            fake_stream.MakeWriteFailureStream(cq, std::move(context), request);
        using Request = ::google::pubsub::v1::StreamingPullRequest;
        // Add expectations for Write() calls with empty subscriptions, only
        // the first call has a non-empty value and it is already set.
        EXPECT_CALL(*stream,
                    Write(Property(&Request::subscription, std::string{}), _))
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.modify_deadline_ack_ids(),
                              ElementsAre("fake-001"));
                  return fake_stream.AddAction("Write");
                });
        return stream;
      });
  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(IsOk())).Times(1);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  fake_stream.WaitForAction().set_value(true);  // Start()
  fake_stream.WaitForAction().set_value(true);  // Write()
  fake_stream.WaitForAction().set_value(true);  // Read()

  auto pending_read = fake_stream.WaitForAction();  // Read() start
  uut->ExtendLeases({"fake-001"}, std::chrono::seconds(10));
  auto pending_write = fake_stream.WaitForAction();  // Write() start

  pending_read.set_value(false);  // Read() done
  shutdown->MarkAsShutdown("test", expected_status);
  uut->Shutdown();

  pending_write.set_value(true);                // Write() done
  fake_stream.WaitForAction().set_value(true);  // Finish()

  EXPECT_EQ(expected_status, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, WriteErrorWaitsForRead) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const expected_status = Status{StatusCode::kNotFound, "gone"};
  FakeStream fake_stream(expected_status);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        auto stream =
            fake_stream.MakeWriteFailureStream(cq, std::move(context), request);
        using Request = ::google::pubsub::v1::StreamingPullRequest;
        // Add expectations for Write() calls with empty subscriptions, only
        // the first call has a non-empty value and it is already set.
        EXPECT_CALL(*stream,
                    Write(Property(&Request::subscription, std::string{}), _))
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_THAT(request.modify_deadline_ack_ids(),
                              ElementsAre("fake-001"));
                  return fake_stream.AddAction("Write");
                });
        return stream;
      });
  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(IsOk())).Times(1);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  fake_stream.WaitForAction().set_value(true);  // Start()
  fake_stream.WaitForAction().set_value(true);  // Write()
  fake_stream.WaitForAction().set_value(true);  // Read()

  auto pending_read = fake_stream.WaitForAction();  // Read() start
  uut->ExtendLeases({"fake-001"}, std::chrono::seconds(10));
  auto pending_write = fake_stream.WaitForAction();  // Write() start

  shutdown->MarkAsShutdown("test", expected_status);
  uut->Shutdown();

  pending_write.set_value(false);               // Write() done
  pending_read.set_value(false);                // Read() done
  fake_stream.WaitForAction().set_value(true);  // Finish()

  EXPECT_EQ(expected_status, done.get());
}

TEST(StreamingSubscriptionBatchSourceTest, ShutdownWithPendingRead) {
  AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto const expected_status = Status{};
  FakeStream fake_stream(expected_status);

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::pubsub::v1::StreamingPullRequest const& request) {
        return fake_stream.MakeWriteFailureStream(cq, std::move(context),
                                                  request);
      });
  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(IsOk())).Times(0);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());
  fake_stream.WaitForAction().set_value(true);      // Start()
  fake_stream.WaitForAction().set_value(true);      // Write()
  auto pending_read = fake_stream.WaitForAction();  // Read() start

  uut->Shutdown();
  shutdown->MarkAsShutdown("test", expected_status);

  pending_read.set_value(true);                 // Read() done
  fake_stream.WaitForAction().set_value(true);  // Finish()
  EXPECT_EQ(expected_status, done.get());
}

/// @test verify that a shutdown cancels the initial Read() call
TEST(StreamingSubscriptionBatchSourceTest, ShutdownWithPendingReadCancel) {
  AutomaticallyCreatedBackgroundThreads background;

  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  AsyncSequencer<bool> async;

  auto wait_and_check_name = [&async](std::string const& name) {
    auto p = async.PopFrontWithName();
    EXPECT_EQ(p.second, name);
    return std::move(p.first);
  };

  auto async_pull_mock = [&](google::cloud::CompletionQueue&,
                             std::unique_ptr<grpc::ClientContext>,
                             google::pubsub::v1::StreamingPullRequest const&) {
    using Response = ::google::pubsub::v1::StreamingPullResponse;
    using Request = ::google::pubsub::v1::StreamingPullRequest;
    auto start_response = [&async] {
      return async.PushBack("Start").then(
          [](future<bool> f) { return f.get(); });
    };
    auto write_response = [&async](Request const&,
                                   grpc::WriteOptions const&) mutable {
      return async.PushBack("Write").then(
          [](future<bool> f) { return f.get(); });
    };
    auto read_response = [&] {
      return async.PushBack("Read").then([](future<bool> f) {
        if (f.get()) return absl::make_optional(Response{});
        return absl::optional<Response>{};
      });
    };
    auto cancel = [&] { async.PushBack("Cancel"); };
    auto finish_response = [&] {
      return async.PushBack("Finish").then(
          [](future<bool>) { return Status{}; });
    };

    auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
    EXPECT_CALL(*stream, Start).WillOnce(start_response);
    EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
    EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
    EXPECT_CALL(*stream, Cancel).WillRepeatedly(cancel);
    EXPECT_CALL(*stream, Finish).WillOnce(finish_response);

    return stream;
  };
  EXPECT_CALL(*mock, AsyncStreamingPull).WillOnce(async_pull_mock);

  using CallbackArg = StatusOr<google::pubsub::v1::StreamingPullResponse>;
  ::testing::MockFunction<void(CallbackArg const&)> callback;
  EXPECT_CALL(callback, Call(IsOk())).Times(0);

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(background.cq(), shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start(callback.AsStdFunction());

  wait_and_check_name("Start").set_value(true);
  wait_and_check_name("Write").set_value(true);
  auto read = wait_and_check_name("Read");

  uut->Shutdown();

  auto cancel = wait_and_check_name("Cancel");
  read.set_value(false);
  shutdown->MarkAsShutdown("test", Status{});
  wait_and_check_name("Finish").set_value(true);
  EXPECT_THAT(done.get(), IsOk());
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

CompletionQueue MakeMockCompletionQueue(AsyncSequencer<bool>& aseq) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([&](std::chrono::nanoseconds) {
        return aseq.PushBack("MakeRelativeTimer").then([](auto) {
          return make_status_or(std::chrono::system_clock::now());
        });
      });
  EXPECT_CALL(*mock_cq, RunAsync)
      .WillRepeatedly([&](std::unique_ptr<RunAsyncBase> f) {
        aseq.PushBack("RunAsync").then([function = std::move(f)](auto) mutable {
          function->exec();
        });
      });
  return CompletionQueue(std::move(mock_cq));
}

TEST(StreamingSubscriptionBatchSourceTest, ExactlyOnceIncludesDeadline) {
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);

  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncStreamingPull)
        .WillOnce([&](google::cloud::CompletionQueue&,
                      std::unique_ptr<grpc::ClientContext>,
                      google::pubsub::v1::StreamingPullRequest const&) {
          auto start_response = [&] {
            return aseq.PushBack("Start").then(
                [](future<bool> g) { return g.get(); });
          };
          auto write_response =
              [&](google::pubsub::v1::StreamingPullRequest const&,
                  grpc::WriteOptions const&) {
                return aseq.PushBack("Write").then(
                    [](future<bool> g) { return g.get(); });
              };
          auto read_response = [&] {
            return aseq.PushBack("Read").then([](future<bool> g) {
              auto ok = g.get();
              using Response = ::google::pubsub::v1::StreamingPullResponse;
              if (!ok) return absl::optional<Response>{};
              Response response;
              response.mutable_subscription_properties()
                  ->set_exactly_once_delivery_enabled(true);
              return absl::make_optional(std::move(response));
            });
          };
          auto finish_response = [&] {
            return aseq.PushBack("Finish").then(
                [](future<bool>) mutable { return Status{}; });
          };

          auto stream =
              absl::make_unique<pubsub_testing::MockAsyncPullStream>();
          EXPECT_CALL(*stream, Start).WillOnce(start_response);
          EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
          EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
          EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
          EXPECT_CALL(*stream, Finish)
              .Times(AtMost(1))
              .WillRepeatedly(finish_response);

          using Request = ::google::pubsub::v1::StreamingPullRequest;
          // Add expectations for Write() calls with empty subscriptions, only
          // the first call has a non-empty value, and that EXPECT_CALL() is
          // already set.
          EXPECT_CALL(*stream,
                      Write(Property(&Request::subscription, std::string{}), _))
              .WillOnce(
                  [&](google::pubsub::v1::StreamingPullRequest const& request,
                      grpc::WriteOptions const&) {
                    EXPECT_EQ(request.stream_ack_deadline_seconds(), 60);
                    EXPECT_THAT(request.modify_deadline_ack_ids(),
                                ElementsAre("fake-006"));
                    EXPECT_THAT(request.modify_deadline_seconds(),
                                ElementsAre(10));
                    EXPECT_THAT(request.ack_ids(), IsEmpty());
                    EXPECT_THAT(request.client_id(), IsEmpty());
                    EXPECT_THAT(request.subscription(), IsEmpty());
                    return aseq.PushBack("Write");
                  });
          return stream;
        });
  }

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(cq, shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  auto timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  aseq.PopFront().set_value(true);  // Start()
  aseq.PopFront().set_value(true);  // Write()
  aseq.PopFront().set_value(true);  // Read()
  auto run = aseq.PopFrontWithName();
  EXPECT_EQ(run.second, "RunAsync");

  uut->ExtendLeases({"fake-006"}, std::chrono::seconds(10));
  aseq.PopFront().set_value(true);  // Write()

  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  run.first.set_value(true);
  aseq.PopFront().set_value(false);  // Read()
  aseq.PopFront().set_value(true);   // Finish()

  EXPECT_THAT(done.get(), IsOk());
}

TEST(StreamingSubscriptionBatchSourceTest, ExactlyOnceDeadlineStateChange) {
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);

  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::StreamingPullRequest const&) {
        auto start_response = [&] {
          return aseq.PushBack("Start").then(
              [](future<bool> g) { return g.get(); });
        };
        auto write_response =
            [&](google::pubsub::v1::StreamingPullRequest const&,
                grpc::WriteOptions const&) {
              return aseq.PushBack("Write").then(
                  [](future<bool> g) { return g.get(); });
            };
        auto read_response_with_eos = [&] {
          return aseq.PushBack("Read").then([](future<bool> g) {
            auto ok = g.get();
            using Response = ::google::pubsub::v1::StreamingPullResponse;
            if (!ok) return absl::optional<Response>{};
            Response response;
            response.mutable_subscription_properties()
                ->set_exactly_once_delivery_enabled(true);
            return absl::make_optional(std::move(response));
          });
        };
        auto finish_response = [&] {
          return aseq.PushBack("Finish").then(
              [](future<bool>) mutable { return Status{}; });
        };

        using Request = ::google::pubsub::v1::StreamingPullRequest;
        auto write_response_with_deadline = [&](Request const& request,
                                                grpc::WriteOptions const&) {
          EXPECT_EQ(request.stream_ack_deadline_seconds(), 60);
          EXPECT_THAT(request.modify_deadline_ack_ids(),
                      ElementsAre("fake-with-stream-deadline"));
          EXPECT_THAT(request.modify_deadline_seconds(), ElementsAre(10));
          EXPECT_THAT(request.ack_ids(), IsEmpty());
          EXPECT_THAT(request.client_id(), IsEmpty());
          EXPECT_THAT(request.subscription(), IsEmpty());
          return aseq.PushBack("Write");
        };
        auto write_response_without_deadline = [&](Request const& request,
                                                   grpc::WriteOptions const&) {
          EXPECT_EQ(request.stream_ack_deadline_seconds(), 0);
          EXPECT_THAT(request.modify_deadline_ack_ids(),
                      ElementsAre("fake-no-stream-deadline"));
          EXPECT_THAT(request.modify_deadline_seconds(), ElementsAre(10));
          EXPECT_THAT(request.ack_ids(), IsEmpty());
          EXPECT_THAT(request.client_id(), IsEmpty());
          EXPECT_THAT(request.subscription(), IsEmpty());
          return aseq.PushBack("Write");
        };

        auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

        ::testing::InSequence sequence;
        EXPECT_CALL(*stream, Start).WillOnce(start_response);
        EXPECT_CALL(*stream, Write).WillOnce(write_response);
        // Two Read() calls with subscription properties, only the first
        // should trigger a `Write()` call that updates the stream deadline.
        EXPECT_CALL(*stream, Read).WillOnce(read_response_with_eos);
        EXPECT_CALL(*stream, Read).WillOnce(read_response_with_eos);
        EXPECT_CALL(*stream, Write).WillOnce(write_response_with_deadline);
        EXPECT_CALL(*stream, Read).WillOnce(read_response_with_eos);
        EXPECT_CALL(*stream, Write).WillOnce(write_response_without_deadline);
        EXPECT_CALL(*stream, Finish)
            .Times(AtMost(1))
            .WillRepeatedly(finish_response);

        return stream;
      });

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(cq, shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  auto timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  aseq.PopFront().set_value(true);  // Start()
  aseq.PopFront().set_value(true);  // Write()

  auto read = aseq.PopFrontWithName();
  EXPECT_EQ(read.second, "Read");
  read.first.set_value(true);

  // The successful Read generates a RunAsync() which generates a Read().
  auto run = aseq.PopFrontWithName();
  EXPECT_EQ(run.second, "RunAsync");
  run.first.set_value(true);
  read = aseq.PopFrontWithName();
  EXPECT_EQ(read.second, "Read");
  // Because Read() changed the subscription properties, this will trigger
  // a Write() call.
  uut->ExtendLeases({"fake-with-stream-deadline"}, std::chrono::seconds(10));
  auto write = aseq.PopFrontWithName();
  EXPECT_EQ(write.second, "Write");
  write.first.set_value(true);  // Write()

  // A second read, but does not change the subscription properties.
  read.first.set_value(true);
  run = aseq.PopFrontWithName();
  EXPECT_EQ(run.second, "RunAsync");
  run.first.set_value(true);
  read = aseq.PopFrontWithName();
  EXPECT_EQ(read.second, "Read");

  uut->ExtendLeases({"fake-no-stream-deadline"}, std::chrono::seconds(10));
  write = aseq.PopFrontWithName();
  EXPECT_EQ(write.second, "Write");
  write.first.set_value(true);  // Write()

  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  read.first.set_value(false);
  aseq.PopFront().set_value(true);  // Finish()

  EXPECT_THAT(done.get(), IsOk());
}

TEST(StreamingSubscriptionBatchSourceTest, AckNackWithRetry) {
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::StreamingPullRequest const&) {
        auto start_response = [&] {
          return aseq.PushBack("Start").then(
              [](future<bool> g) { return g.get(); });
        };
        auto write_response =
            [&](google::pubsub::v1::StreamingPullRequest const&,
                grpc::WriteOptions const&) {
              return aseq.PushBack("Write").then(
                  [](future<bool> g) { return g.get(); });
            };
        auto read_response_with_eos = [&] {
          return aseq.PushBack("Read").then([](future<bool> g) {
            auto ok = g.get();
            using Response = ::google::pubsub::v1::StreamingPullResponse;
            if (!ok) return absl::optional<Response>{};
            Response response;
            response.mutable_subscription_properties()
                ->set_exactly_once_delivery_enabled(true);
            return absl::make_optional(std::move(response));
          });
        };
        auto finish_response = [&] {
          return aseq.PushBack("Finish").then(
              [](future<bool>) mutable { return Status{}; });
        };

        auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

        ::testing::InSequence sequence;
        EXPECT_CALL(*stream, Start).WillOnce(start_response);
        EXPECT_CALL(*stream, Write).WillOnce(write_response);
        // Two Read() calls with subscription properties, only the first should
        // trigger a `Write()` call that updates the stream deadline.
        EXPECT_CALL(*stream, Read).WillOnce(read_response_with_eos);
        EXPECT_CALL(*stream, Read).WillOnce(read_response_with_eos);
        EXPECT_CALL(*stream, Finish)
            .Times(AtMost(1))
            .WillRepeatedly(finish_response);

        return stream;
      });

  EXPECT_CALL(
      *mock, AsyncAcknowledge(
                 _, _, Property(&AckRequest::ack_ids, ElementsAre("fake-001"))))
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "try-again")))))
      .WillOnce(Return(ByMove(make_ready_future(
          Status(StatusCode::kUnknown, "uh?",
                 ErrorInfo("test-only-reason", "test-only-domain",
                           {{"fake-001", "TRANSIENT_FAILURE_BLAH_BLAH"}}))))))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _,
                                            Property(&ModifyRequest::ack_ids,
                                                     ElementsAre("fake-002"))))
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "try-again")))))
      .WillOnce(Return(ByMove(make_ready_future(
          Status(StatusCode::kUnknown, "uh?",
                 ErrorInfo("test-only-reason", "test-only-domain",
                           {{"fake-002", "TRANSIENT_FAILURE_BLAH_BLAH"}}))))))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = MakeTestBatchSource(cq, shutdown, mock);

  auto done = shutdown->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  auto timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  aseq.PopFront().set_value(true);  // Start()
  aseq.PopFront().set_value(true);  // Write()

  auto read = aseq.PopFrontWithName();
  EXPECT_EQ(read.second, "Read");
  read.first.set_value(true);

  // The successful Read() generates a RunAsync() which generates a Read().
  auto run = aseq.PopFrontWithName();
  EXPECT_EQ(run.second, "RunAsync");
  run.first.set_value(true);
  read = aseq.PopFrontWithName();
  EXPECT_EQ(read.second, "Read");

  auto ack = uut->AckMessage("fake-001");
  auto backoff = aseq.PopFrontWithName();
  EXPECT_EQ(backoff.second, "MakeRelativeTimer");
  backoff.first.set_value(true);
  backoff = aseq.PopFrontWithName();
  EXPECT_EQ(backoff.second, "MakeRelativeTimer");
  backoff.first.set_value(true);
  EXPECT_STATUS_OK(ack.get());

  auto nack = uut->NackMessage("fake-002");
  backoff = aseq.PopFrontWithName();
  EXPECT_EQ(backoff.second, "MakeRelativeTimer");
  backoff.first.set_value(true);
  backoff = aseq.PopFrontWithName();
  EXPECT_EQ(backoff.second, "MakeRelativeTimer");
  backoff.first.set_value(true);
  EXPECT_STATUS_OK(nack.get());

  shutdown->MarkAsShutdown("test", {});
  uut->Shutdown();
  read.first.set_value(false);
  aseq.PopFront().set_value(true);  // Finish()

  EXPECT_THAT(done.get(), IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
