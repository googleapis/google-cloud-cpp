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

#include "google/cloud/internal/async_rest_polling_loop_custom.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::ImmutableOptions;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::longrunning::Operation;
using ::testing::Eq;
using ::testing::Return;
using TimerType = StatusOr<std::chrono::system_clock::time_point>;
using Response = google::protobuf::Timestamp;
using Request = google::protobuf::Duration;

struct StringOption {
  using Type = std::string;
};

class MockStub {
 public:
  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               google::longrunning::GetOperationRequest const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               google::longrunning::CancelOperationRequest const&),
              ());
};

class MockPollingPolicy : public PollingPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<PollingPolicy>, clone, (), (const, override));
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(std::chrono::milliseconds, WaitPeriod, (), (override));
};

std::string CurrentTestName() {
  return testing::UnitTest::GetInstance()->current_test_info()->name();
}

class BespokeOperationType {
 public:
  bool is_done() const { return is_done_; }
  void set_is_done(bool is_done) { is_done_ = is_done; }
  std::string const& name() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }
  std::string* mutable_name() { return &name_; }
  bool operator==(BespokeOperationType const& other) const {
    return is_done_ == other.is_done_ && name_ == other.name_;
  }

 private:
  bool is_done_;
  std::string name_;
};

class BespokeOperationTypeNoNameMethod {
 public:
  bool is_done() const { return is_done_; }
  void set_is_done(bool is_done) { is_done_ = is_done; }
  std::string const& pseudo_name_function() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }
  std::string* mutable_name() { return &name_; }
  bool operator==(BespokeOperationTypeNoNameMethod const& other) const {
    return is_done_ == other.is_done_ && name_ == other.name_;
  }

 private:
  bool is_done_;
  std::string name_;
};

class BespokeGetOperationRequestType {
 public:
  std::string const& name() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }

 private:
  std::string name_;
};

class BespokeCancelOperationRequestType {
 public:
  std::string const& name() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }

 private:
  std::string name_;
};

class MockBespokeOperationStub {
 public:
  MOCK_METHOD(future<StatusOr<BespokeOperationType>>, AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               BespokeGetOperationRequestType const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               BespokeCancelOperationRequestType const&),
              ());
};

TEST(AsyncRestPollingLoopTest, PollThenSuccessWithBespokeOperationTypes) {
  Response response;
  response.set_seconds(123456);
  BespokeOperationType starting_op;
  starting_op.set_name("test-op-name");
  starting_op.set_is_done(false);
  BespokeOperationType expected = starting_op;
  expected.set_is_done(true);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBespokeOperationStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    ImmutableOptions const& options,
                    BespokeGetOperationRequestType const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto pending =
      AsyncRestPollingLoop<BespokeOperationType, BespokeGetOperationRequestType,
                           BespokeCancelOperationRequestType>(
          std::move(cq), current,
          make_ready_future(make_status_or(starting_op)),
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 ImmutableOptions options,
                 BespokeGetOperationRequestType const& request) {
            return mock->AsyncGetOperation(cq, std::move(context),
                                           std::move(options), request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 ImmutableOptions options,
                 BespokeCancelOperationRequestType const& request) {
            return mock->AsyncCancelOperation(cq, std::move(context),
                                              std::move(options), request);
          },
          std::move(policy), "test-function",
          [](BespokeOperationType const& op) { return op.is_done(); },
          [](std::string const& s, BespokeGetOperationRequestType& op) {
            op.set_name(s);
          },
          [](std::string const& s, BespokeCancelOperationRequestType& op) {
            op.set_name(s);
          });
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  StatusOr<BespokeOperationType> actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, Eq(expected));
}

class MockBespokeOperationNoNameMethodStub {
 public:
  MOCK_METHOD(future<StatusOr<BespokeOperationTypeNoNameMethod>>,
              AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               BespokeGetOperationRequestType const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               BespokeCancelOperationRequestType const&),
              ());
};

TEST(AsyncRestPollingLoopTest,
     PollThenSuccessWithBespokeOperationTypeNoNameMethod) {
  Response response;
  response.set_seconds(123456);
  BespokeOperationTypeNoNameMethod starting_op;
  starting_op.set_name("test-op-name");
  starting_op.set_is_done(false);
  BespokeOperationTypeNoNameMethod expected = starting_op;
  expected.set_is_done(true);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBespokeOperationNoNameMethodStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    ImmutableOptions const& options,
                    BespokeGetOperationRequestType const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncRestPollingLoop<BespokeOperationTypeNoNameMethod,
                                      BespokeGetOperationRequestType,
                                      BespokeCancelOperationRequestType>(
      std::move(cq), current, make_ready_future(make_status_or(starting_op)),
      [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             ImmutableOptions options,
             BespokeGetOperationRequestType const& request) {
        return mock->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             ImmutableOptions options,
             BespokeCancelOperationRequestType const& request) {
        return mock->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      std::move(policy), "test-function",
      [](BespokeOperationTypeNoNameMethod const& op) { return op.is_done(); },
      [](std::string const& s, BespokeGetOperationRequestType& op) {
        op.set_name(s);
      },
      [](std::string const& s, BespokeCancelOperationRequestType& op) {
        op.set_name(s);
      },
      [](StatusOr<BespokeOperationTypeNoNameMethod> const& op) {
        return op->pseudo_name_function();
      });
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  StatusOr<BespokeOperationTypeNoNameMethod> actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, Eq(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
