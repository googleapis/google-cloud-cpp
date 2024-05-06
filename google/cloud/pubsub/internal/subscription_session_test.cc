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

#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/application_callback.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/testing/fake_streaming_pull.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub::ExactlyOnceAckHandler;
using ::google::cloud::pubsub_testing::FakeAsyncStreamingPull;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Contains;
using ::testing::HasSubstr;

future<Status> CreateTestSubscriptionSession(
    pubsub::Subscription const& subscription, Options opts,
    std::shared_ptr<SubscriberStub> const& mock, CompletionQueue const& cq,
    pubsub::SubscriberConnection::SubscribeParams p) {
  opts.set<pubsub::SubscriptionOption>(subscription);
  opts = DefaultSubscriberOptions(
      pubsub_testing::MakeTestOptions(std::move(opts)));
  return CreateSubscriptionSession(std::move(opts), mock, cq, "test-client-id",
                                   std::move(p.callback));
}

future<Status> CreateTestSubscriptionSession(
    pubsub::Subscription const& subscription, Options opts,
    std::shared_ptr<SubscriberStub> const& mock, CompletionQueue const& cq,
    pubsub::ExactlyOnceApplicationCallback callback) {
  opts.set<pubsub::SubscriptionOption>(subscription);
  opts = DefaultSubscriberOptions(
      pubsub_testing::MakeTestOptions(std::move(opts)));
  return CreateSubscriptionSession(std::move(opts), mock, cq, "test-client-id",
                                   std::move(callback));
}

void ScheduleCallbacks(int64_t ack_count, bool enable_open_telemetry) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  std::mutex mu;
  int count = 0;
  std::mutex ack_id_mu;
  std::condition_variable ack_id_cv;
  int expected_ack_id = 0;

  google::cloud::CompletionQueue cq;
  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
  using us = std::chrono::microseconds;
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly(
          [&](google::cloud::CompletionQueue&, auto, auto,
              google::pubsub::v1::AcknowledgeRequest const& request) {
            for (auto const& a : request.ack_ids()) {
              std::lock_guard<std::mutex> lk(ack_id_mu);
              EXPECT_EQ("test-ack-id-" + std::to_string(expected_ack_id), a);
              ++expected_ack_id;
              if (expected_ack_id >= ack_count) ack_id_cv.notify_one();
            }
            return cq.MakeRelativeTimer(us(10)).then(
                [](TimerFuture) { return Status{}; });
          });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue const&, auto, auto) {
        auto stream = std::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return cq.MakeRelativeTimer(us(10)).then(
              [](TimerFuture) { return true; });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_EQ(subscription.FullName(), request.subscription());
                  EXPECT_TRUE(request.ack_ids().empty());
                  EXPECT_TRUE(request.modify_deadline_ack_ids().empty());
                  EXPECT_TRUE(request.modify_deadline_seconds().empty());
                  return cq.MakeRelativeTimer(us(10)).then(
                      [](TimerFuture) { return true; });
                });

        ::testing::InSequence sequence;
        EXPECT_CALL(*stream, Read).WillRepeatedly([&] {
          google::pubsub::v1::StreamingPullResponse response;
          for (int i = 0; i != 2; ++i) {
            auto& m = *response.add_received_messages();
            std::lock_guard<std::mutex> lk(mu);
            m.set_ack_id("test-ack-id-" + std::to_string(count));
            m.set_delivery_attempt(42);
            m.mutable_message()->set_message_id("test-message-id-" +
                                                std::to_string(count));
            ++count;
          }
          return cq.MakeRelativeTimer(us(10)).then([response](TimerFuture) {
            return absl::make_optional(response);
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Read).WillRepeatedly([&] {
          return cq.MakeRelativeTimer(us(10)).then([](TimerFuture) {
            return absl::optional<google::pubsub::v1::StreamingPullResponse>{};
          });
        });
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return cq.MakeRelativeTimer(us(10)).then([](TimerFuture) {
            return Status{StatusCode::kCancelled, "cancel"};
          });
        });

        return stream;
      });

  std::vector<std::thread> tasks;
  std::generate_n(std::back_inserter(tasks), 4,
                  [&] { return std::thread([&cq] { cq.Run(); }); });
  std::set<std::thread::id> ids;
  auto const main_id = std::this_thread::get_id();
  std::transform(tasks.begin(), tasks.end(), std::inserter(ids, ids.end()),
                 [](std::thread const& t) { return t.get_id(); });

  std::atomic<int> expected_message_id{0};
  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    EXPECT_EQ(42, h.delivery_attempt());
    EXPECT_EQ("test-message-id-" + std::to_string(expected_message_id),
              m.message_id());
    auto pos = ids.find(std::this_thread::get_id());
    EXPECT_NE(ids.end(), pos);
    EXPECT_NE(main_id, std::this_thread::get_id());
    // Increment the counter before acking, as the ack() may trigger a new call
    // before this function gets to run.
    ++expected_message_id;
    std::move(h).ack();
  };

  auto response = CreateTestSubscriptionSession(
      subscription,
      Options{}
          .set<pubsub::MaxConcurrencyOption>(1)
          .set<OpenTelemetryTracingOption>(enable_open_telemetry),
      mock, cq, {handler});
  {
    std::unique_lock<std::mutex> lk(ack_id_mu);
    ack_id_cv.wait(lk, [&] { return expected_ack_id >= ack_count; });
  }
  response.cancel();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  for (auto& t : tasks) t.join();
}

/// @test Verify callbacks are scheduled in the background threads.
TEST(SubscriptionSessionTest, ScheduleCallbacks) {
  ScheduleCallbacks(/*ack_count=*/100, /*enable_open_telemetry=*/false);
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsInternal;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::Contains;
using ::testing::Ge;
using ::testing::SizeIs;

/// @test Verify callbacks are scheduled in the background threads with Open
/// Telemetry enabled.
TEST(SubscriptionSessionTest, ScheduleCallbacksWithOtelEnabled) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto constexpr kAckCount = 100;
  ScheduleCallbacks(kAckCount, /*enable_open_telemetry=*/true);

  auto spans = span_catcher->GetSpans();
  // There should be a process and ack span for each message.
  EXPECT_THAT(spans, SizeIs(Ge(2 * kAckCount)));
  // Verify there is at least one process span.
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                            SpanNamed("test-subscription process"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                sc::kMessagingSystem, "gcp_pubsub")))));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

/// @test Verify callbacks are scheduled in the background threads.
TEST(SubscriptionSessionTest, ScheduleCallbacksExactlyOnce) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  std::mutex mu;
  int count = 0;
  std::mutex ack_id_mu;
  std::condition_variable ack_id_cv;
  int expected_ack_id = 0;
  auto constexpr kAckCount = 100;

  google::cloud::CompletionQueue cq;
  using us = std::chrono::microseconds;
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly(
          [&](google::cloud::CompletionQueue&, auto, auto,
              google::pubsub::v1::AcknowledgeRequest const& request) {
            for (auto const& a : request.ack_ids()) {
              std::lock_guard<std::mutex> lk(ack_id_mu);
              EXPECT_EQ("test-ack-id-" + std::to_string(expected_ack_id), a);
              ++expected_ack_id;
              if (expected_ack_id >= kAckCount) ack_id_cv.notify_one();
            }
            return cq.MakeRelativeTimer(us(10)).then(
                [](auto) { return Status{}; });
          });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue const&, auto, auto) {
        auto stream = std::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return cq.MakeRelativeTimer(us(10)).then([](auto) { return true; });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::pubsub::v1::StreamingPullRequest const& request,
                    grpc::WriteOptions const&) {
                  EXPECT_EQ(subscription.FullName(), request.subscription());
                  EXPECT_TRUE(request.ack_ids().empty());
                  EXPECT_TRUE(request.modify_deadline_ack_ids().empty());
                  EXPECT_TRUE(request.modify_deadline_seconds().empty());
                  return cq.MakeRelativeTimer(us(10)).then(
                      [](auto) { return true; });
                });

        ::testing::InSequence sequence;
        EXPECT_CALL(*stream, Read).WillRepeatedly([&] {
          google::pubsub::v1::StreamingPullResponse response;
          for (int i = 0; i != 2; ++i) {
            auto& m = *response.add_received_messages();
            std::lock_guard<std::mutex> lk(mu);
            m.set_ack_id("test-ack-id-" + std::to_string(count));
            m.set_delivery_attempt(42);
            m.mutable_message()->set_message_id("test-message-id-" +
                                                std::to_string(count));
            ++count;
          }
          return cq.MakeRelativeTimer(us(10)).then(
              [response](auto) { return absl::make_optional(response); });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Read).WillRepeatedly([&] {
          return cq.MakeRelativeTimer(us(10)).then([](auto) {
            return absl::optional<google::pubsub::v1::StreamingPullResponse>{};
          });
        });
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return cq.MakeRelativeTimer(us(10)).then(
              [](auto) { return Status{StatusCode::kCancelled, "cancel"}; });
        });

        return stream;
      });

  std::vector<std::thread> tasks;
  std::generate_n(std::back_inserter(tasks), 4,
                  [&] { return std::thread([&cq] { cq.Run(); }); });
  std::set<std::thread::id> ids;
  auto const main_id = std::this_thread::get_id();
  std::transform(tasks.begin(), tasks.end(), std::inserter(ids, ids.end()),
                 [](std::thread const& t) { return t.get_id(); });

  std::atomic<int> expected_message_id{0};
  auto callback = [&](pubsub::Message const& m, ExactlyOnceAckHandler h) {
    EXPECT_EQ(42, h.delivery_attempt());
    EXPECT_EQ("test-message-id-" + std::to_string(expected_message_id),
              m.message_id());
    auto pos = ids.find(std::this_thread::get_id());
    EXPECT_NE(ids.end(), pos);
    EXPECT_NE(main_id, std::this_thread::get_id());
    // Increment the counter before acking, as the ack() may trigger a new call
    // before this function gets to run.
    ++expected_message_id;
    std::move(h).ack();
  };

  auto response = CreateTestSubscriptionSession(
      subscription, Options{}.set<pubsub::MaxConcurrencyOption>(1), mock, cq,
      callback);
  {
    std::unique_lock<std::mutex> lk(ack_id_mu);
    ack_id_cv.wait(lk, [&] { return expected_ack_id >= kAckCount; });
  }
  response.cancel();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  for (auto& t : tasks) t.join();
}

/// @test Verify ack/nack errors are delivered to the application.
TEST(SubscriptionSessionTest, ExactlyOnceAckErrors) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const& r) {
        if (r.ack_ids_size() == 1 && r.ack_ids(0) == "test-ack-id-0") {
          return make_ready_future(Status{StatusCode::kUnauthenticated, "0"});
        }
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly(
          [](google::cloud::CompletionQueue&, auto, auto,
             google::pubsub::v1::ModifyAckDeadlineRequest const& r) {
            if (r.ack_ids_size() == 1 && r.ack_ids(0) == "test-ack-id-1") {
              return make_ready_future(
                  Status{StatusCode::kPermissionDenied, "1"});
            }
            return make_ready_future(Status{});
          });

  promise<void> enough_messages;
  std::atomic<int> received_counter{0};
  auto constexpr kMaximumMessages = 9;
  auto callback = [&](pubsub::Message const& m, ExactlyOnceAckHandler h) {
    auto c = received_counter.load();
    EXPECT_LE(c, kMaximumMessages);
    SCOPED_TRACE("Running for message " + m.message_id() +
                 ", counter=" + std::to_string(c));
    EXPECT_EQ("test-message-id-" + std::to_string(c), m.message_id());
    if (++received_counter == kMaximumMessages) {
      enough_messages.set_value();
    }
    if (m.message_id() == "test-message-id-0") {
      std::move(h).ack().then([m](auto f) {
        EXPECT_EQ(f.get(), Status(StatusCode::kUnauthenticated, "0"));
      });
    } else if (m.message_id() == "test-message-id-1") {
      std::move(h).nack().then([m](auto f) {
        EXPECT_EQ(f.get(), Status(StatusCode::kPermissionDenied, "1"));
      });
    } else {
      std::move(h).ack();
    }
  };

  google::cloud::CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });
  auto response = CreateTestSubscriptionSession(
      subscription, Options{}.set<pubsub::MaxConcurrencyOption>(1), mock, cq,
      callback);
  enough_messages.get_future()
      .then([&](future<void>) { response.cancel(); })
      .get();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  t.join();
}

/// @test Verify ack/nack errors are logged if the application ignores them.
TEST(SubscriptionSessionTest, DefaultAckHandlerLogsErrors) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const& r) {
        if (r.ack_ids_size() == 1 && r.ack_ids(0) == "test-ack-id-0") {
          return make_ready_future(Status{StatusCode::kUnauthenticated,
                                          "some error for test-ack-id-0"});
        }
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly(
          [](google::cloud::CompletionQueue&, auto, auto,
             google::pubsub::v1::ModifyAckDeadlineRequest const& r) {
            if (r.ack_ids_size() == 1 && r.ack_ids(0) == "test-ack-id-1") {
              return make_ready_future(Status{StatusCode::kPermissionDenied,
                                              "some error for test-ack-id-1"});
            }
            return make_ready_future(Status{});
          });

  promise<void> enough_messages;
  std::atomic<int> received_counter{0};
  auto constexpr kMaximumMessages = 9;
  auto callback = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    auto c = received_counter.load();
    EXPECT_LE(c, kMaximumMessages);
    SCOPED_TRACE("Running for message " + m.message_id() +
                 ", counter=" + std::to_string(c));
    EXPECT_EQ("test-message-id-" + std::to_string(c), m.message_id());
    if (++received_counter == kMaximumMessages) {
      enough_messages.set_value();
    }
    if (m.message_id() == "test-message-id-1") {
      std::move(h).nack();
      return;
    }
    std::move(h).ack();
  };

  ScopedLog log;
  google::cloud::CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });
  auto response = CreateTestSubscriptionSession(
      subscription, Options{}.set<pubsub::MaxConcurrencyOption>(1), mock, cq,
      {callback});
  enough_messages.get_future()
      .then([&](future<void>) { response.cancel(); })
      .get();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  t.join();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(AllOf(HasSubstr(" ack()"), HasSubstr("test-message-id-0"),
                     HasSubstr("some error for test-ack-id-0"))));
  EXPECT_THAT(
      log_lines,
      Contains(AllOf(HasSubstr(" nack()"), HasSubstr("test-message-id-1"),
                     HasSubstr("some error for test-ack-id-1"))));
}

/// @test Verify callbacks are scheduled in sequence.
TEST(SubscriptionSessionTest, SequencedCallbacks) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  promise<void> enough_messages;
  std::atomic<int> received_counter{0};
  auto constexpr kMaximumMessages = 9;
  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    auto c = received_counter.load();
    EXPECT_LE(c, kMaximumMessages);
    SCOPED_TRACE("Running for message " + m.message_id() +
                 ", counter=" + std::to_string(c));
    EXPECT_EQ("test-message-id-" + std::to_string(c), m.message_id());
    if (++received_counter == kMaximumMessages) {
      enough_messages.set_value();
    }
    std::move(h).ack();
  };

  google::cloud::CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });
  auto response = CreateTestSubscriptionSession(
      subscription, Options{}.set<pubsub::MaxConcurrencyOption>(1), mock, cq,
      {handler});
  enough_messages.get_future()
      .then([&](future<void>) { response.cancel(); })
      .get();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  t.join();
}

/// @test Verify pending callbacks are nacked on shutdown.
TEST(SubscriptionSessionTest, ShutdownNackCallbacks) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  // Now unto the handler, basically it counts messages and from the second one
  // onwards it just nacks.
  promise<void> enough_messages;
  std::atomic<int> ack_count{0};
  auto constexpr kMaximumAcks = 2;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    auto count = ++ack_count;
    if (count == kMaximumAcks) enough_messages.set_value();
    std::move(h).ack();
  };

  google::cloud::CompletionQueue cq;
  auto response = CreateTestSubscriptionSession(
      subscription,
      Options{}
          .set<pubsub::MaxOutstandingMessagesOption>(1)
          .set<pubsub::MaxOutstandingBytesOption>(1)
          .set<pubsub::MaxDeadlineTimeOption>(std::chrono::seconds(60)),
      mock, cq, {handler});
  // Setup the system to cancel after the second message.
  auto done = enough_messages.get_future().then(
      [&](future<void>) { response.cancel(); });
  std::thread t{[&cq] { cq.Run(); }};
  done.get();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  t.join();
}

/// @test Verify shutting down a session waits for pending tasks.
TEST(SubscriptionSessionTest, ShutdownWaitsFutures) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  auto constexpr kMaximumAcks = 10;

  internal::AutomaticallyCreatedBackgroundThreads background;
  std::atomic<int> handler_counter{0};

  // Create a scope for the handler and its variables, this makes it easier to
  // discover bugs under TSAN/ASAN.
  {
    // Now unto the handler, basically it counts messages and nacks starting at
    // kMaximumAcks.
    promise<void> got_one;
    auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
      if (handler_counter.load() == 0) got_one.set_value();
      if (++handler_counter > kMaximumAcks) return;
      std::move(h).ack();
    };

    auto session = CreateTestSubscriptionSession(subscription, Options{}, mock,
                                                 background.cq(), {handler});
    got_one.get_future()
        .then([&session](future<void>) { session.cancel(); })
        .get();

    auto status = session.get();
    EXPECT_STATUS_OK(status);
    EXPECT_LE(1, handler_counter.load());
  }
  // Schedule at least a few more iterations of the CQ loop. If the shutdown is
  // buggy, we will see TSAN/ASAN errors because the `handler` defined above
  // is still called.
  auto const initial_value = handler_counter.load();
  for (int i = 0; i != 10; ++i) {
    SCOPED_TRACE("Wait loop iteration " + std::to_string(i));
    promise<void> done;
    background.cq().RunAsync([&done] { done.set_value(); });
    done.get_future().get();
  }
  auto const final_value = handler_counter.load();
  EXPECT_EQ(initial_value, final_value);
}

/// @test Verify shutting down a session waits for pending tasks.
TEST(SubscriptionSessionTest, ShutdownWaitsConditionVars) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  // A number of mocks that return futures satisfied a bit after the call is
  // made. This better simulates the behavior when running against an actual
  // service.
  auto constexpr kMaximumAcks = 20;

  internal::AutomaticallyCreatedBackgroundThreads background;
  std::atomic<int> handler_counter{0};

  // Create a scope for the handler and its variables, makes the errors more
  // obvious under TSAN/ASAN.
  {
    // Now unto the handler, basically it counts messages and nacks starting at
    // kMaximumAcks.
    std::mutex mu;
    std::condition_variable cv;
    int ack_count = 0;
    auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
      ++handler_counter;
      std::unique_lock<std::mutex> lk(mu);
      if (++ack_count > kMaximumAcks) return;
      lk.unlock();
      cv.notify_one();
      std::move(h).ack();
    };

    auto session = CreateTestSubscriptionSession(subscription, Options{}, mock,
                                                 background.cq(), {handler});
    {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [&] { return ack_count >= kMaximumAcks; });
    }
    session.cancel();
    auto status = session.get();
    EXPECT_STATUS_OK(status);
  }
  // Schedule at least a few more iterations of the CQ loop. If the shutdown is
  // buggy, we will see TSAN/ASAN errors because the `handler` defined above
  // is still called.
  auto const initial_value = handler_counter.load();
  for (int i = 0; i != 10; ++i) {
    SCOPED_TRACE("Wait loop iteration " + std::to_string(i));
    promise<void> done;
    background.cq().RunAsync([&done] { done.set_value(); });
    done.get_future().get();
  }
  auto const final_value = handler_counter.load();
  EXPECT_EQ(initial_value, final_value);
}

/// @test Verify shutting down a session waits for pending tasks.
TEST(SubscriptionSessionTest, ShutdownWaitsEarlyAcks) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  auto constexpr kMessageCount = 16;

  internal::AutomaticallyCreatedBackgroundThreads background(kMessageCount);
  std::atomic<int> handler_counter{0};

  // Create a scope for the handler and its variables, which makes the errors
  // more obvious under TSAN/ASAN.
  {
    // Now unto the handler. It counts messages, and uses objects after the
    // AckHandler has been returned. If the session shutdown is not working
    // correctly using these variables is a problem under TSAN and ASAN. We also
    // have a more direct detection of problems later in this test.
    std::mutex mu;
    std::condition_variable cv;
    int ack_count = 0;
    auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
      std::move(h).ack();
      // Sleep after the `ack()` call to more easily reproduce #5148
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      ++handler_counter;
      {
        std::unique_lock<std::mutex> lk(mu);
        ++ack_count;
      }
      cv.notify_one();
    };

    auto session = CreateTestSubscriptionSession(
        subscription,
        Options{}.set<pubsub::MaxConcurrencyOption>(2 * kMessageCount), mock,
        background.cq(), {handler});
    {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [&] { return ack_count >= kMessageCount; });
    }
    session.cancel();
    auto status = session.get();
    EXPECT_STATUS_OK(status);
  }
  // Schedule at least a few more iterations of the CQ loop. If the shutdown is
  // buggy, we will see TSAN/ASAN errors because the `handler` defined above
  // is still called.
  auto const initial_value = handler_counter.load();
  for (std::size_t i = 0; i != 10 * background.pool_size(); ++i) {
    SCOPED_TRACE("Wait loop iteration " + std::to_string(i));
    promise<void> done;
    background.cq().RunAsync([&done] { done.set_value(); });
    done.get_future().get();
  }
  auto const final_value = handler_counter.load();
  EXPECT_EQ(initial_value, final_value);
}

/// @test Verify sessions continue even if future is released.
TEST(SubscriptionSessionTest, FireAndForget) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  std::mutex mu;
  std::condition_variable cv;
  int ack_count = 0;
  Status status;
  auto wait_ack_count = [&](int n) {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return ack_count >= n || !status.ok(); });
    return ack_count;
  };

  auto constexpr kMessageCount = 8;
  // Create a scope for the background completion queues and threads.
  {
    internal::AutomaticallyCreatedBackgroundThreads background;

    // Create a scope so the future and handler get destroyed, but we want the
    // test to continue afterwards.
    {
      auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
        std::move(h).ack();
        std::unique_lock<std::mutex> lk(mu);
        if (++ack_count % kMessageCount == 0) cv.notify_one();
      };

      (void)CreateTestSubscriptionSession(
          subscription,
          Options{}
              .set<pubsub::MaxOutstandingMessagesOption>(kMessageCount / 2)
              .set<pubsub::MaxConcurrencyOption>(kMessageCount / 2)
              .set<pubsub::ShutdownPollingPeriodOption>(
                  std::chrono::milliseconds(20)),
          mock, background.cq(), {handler})
          .then([&](future<Status> f) {
            std::unique_lock<std::mutex> lk(mu);
            status = f.get();
            cv.notify_one();
          });
      wait_ack_count(kMessageCount);
    }

    auto const initial_value = wait_ack_count(2 * kMessageCount);
    auto const final_value = wait_ack_count(initial_value + 2 * kMessageCount);
    EXPECT_NE(initial_value, final_value);
    std::unique_lock<std::mutex> lk(mu);
    EXPECT_STATUS_OK(status);
  }
}

/// @test Verify sessions shutdown properly even if future is released.
TEST(SubscriptionSessionTest, FireAndForgetShutdown) {
  pubsub::Subscription const subscription("test-project", "test-subscription");

  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  AsyncSequencer<bool> on_read;
  AsyncSequencer<Status> on_finish;

  auto async_pull_mock = [&](google::cloud::CompletionQueue const& cq, auto,
                             auto) {
    using us = std::chrono::microseconds;
    using F = future<StatusOr<std::chrono::system_clock::time_point>>;
    using Response = ::google::pubsub::v1::StreamingPullResponse;
    auto start_response = [q = cq]() mutable {
      return q.MakeRelativeTimer(us(10)).then([](F) { return true; });
    };
    auto write_response = [q = cq](
                              google::pubsub::v1::StreamingPullRequest const&,
                              grpc::WriteOptions const&) mutable {
      return q.MakeRelativeTimer(us(10)).then([](F) { return true; });
    };
    auto read_response = [&] {
      return on_read.PushBack("Read").then([](future<bool> f) {
        if (f.get()) return absl::make_optional(Response{});
        return absl::optional<Response>{};
      });
    };
    auto finish_response = [&] {
      return on_finish.PushBack("Finish").then(
          [](future<Status> f) { return f.get(); });
    };

    auto stream = std::make_unique<pubsub_testing::MockAsyncPullStream>();
    EXPECT_CALL(*stream, Start).WillOnce(start_response);
    EXPECT_CALL(*stream, Write).WillRepeatedly(write_response);
    EXPECT_CALL(*stream, Read).WillRepeatedly(read_response);
    EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
    EXPECT_CALL(*stream, Finish).WillOnce(finish_response);

    return stream;
  };
  EXPECT_CALL(*mock, AsyncStreamingPull).WillRepeatedly(async_pull_mock);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  promise<Status> shutdown_completed;
  internal::AutomaticallyCreatedBackgroundThreads background(1);
  {
    auto handler = [&](pubsub::Message const&, pubsub::AckHandler) {};
    (void)CreateTestSubscriptionSession(
        subscription,
        Options{}.set<pubsub::ShutdownPollingPeriodOption>(
            std::chrono::milliseconds(100)),
        mock, background.cq(), {handler})
        .then([&shutdown_completed](future<Status> f) {
          shutdown_completed.set_value(f.get());
        });
  }
  // Make the first Read() call fail and then wait before returning from
  // Finish()
  on_read.PopFront().set_value(false);
  auto finish = on_finish.PopFront();
  // Shutdown the completion queue, this will disable the timers for the second
  // async_pull
  background.cq().Shutdown();
  finish.set_value(Status{});

  // At this point the streaming pull cannot restart, so there are no pending
  // operations.  Eventually the session will be finished.
  shutdown_completed.get_future().get();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
