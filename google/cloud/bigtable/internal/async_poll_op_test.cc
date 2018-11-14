// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <future>
#include <thread>
#include <typeinfo>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
namespace {

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;

class NoexTableAsyncPollOpTest
    : public bigtable::testing::internal::TableTestFixture {};

using Functor = std::function<void(CompletionQueue&, bool, grpc::Status&)>;

// Operation passed to AsyncPollOp needs to be movable or copyable. Mocked
// objects are neither, so we'll mock this class and hold a shared_ptr to it in
// DummyOperation. DummyOperation will satisfy the requirements for a parameter
// to AsyncPollOp.
// Also, gmock doesn't support rvalue references and templates, so workaround
// that too.
class DummyOperationImpl {
 public:
  virtual ~DummyOperationImpl() = default;
  virtual std::shared_ptr<AsyncOperation> Start(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>&,
      Functor const&) = 0;
  virtual int AccumulatedResult() = 0;
};

class DummyOperation {
 public:
  using Response = int;

  DummyOperation(std::shared_ptr<DummyOperationImpl> impl) : impl_(impl) {}

  template <typename F,
            typename std::enable_if<
                google::cloud::internal::is_invocable<F, CompletionQueue&, bool,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext>&& context,
      F&& callback) {
    std::unique_ptr<grpc::ClientContext> context_moved(std::move(context));
    return impl_->Start(cq, context_moved, std::move(callback));
  }

  virtual int AccumulatedResult() { return impl_->AccumulatedResult(); }

 private:
  std::shared_ptr<DummyOperationImpl> impl_;
};

class AsyncOperationMock : public AsyncOperation {
 public:
  bool Notify(CompletionQueue& cq, bool ok) override {
    // TODO(#1389) Notify should be moved from AsyncOperation to some more
    // specific derived class.
    google::cloud::internal::RaiseLogicError(
        "This member function doesn't make sense in "
        "AsyncPollOp");
  }
  MOCK_METHOD0(Cancel, void());
};

class DummyOperationMock : public DummyOperationImpl {
 public:
  MOCK_METHOD3(Start,
               std::shared_ptr<AsyncOperation>(
                   CompletionQueue&, std::unique_ptr<grpc::ClientContext>&,
                   Functor const&));
  MOCK_METHOD0(AccumulatedResult, int());
};

class ImmediateFinishTestConfig {
 public:
  // Error code returned from the first Start()
  grpc::StatusCode error_code;
  // finished flag returned from the second Start()
  bool finished;
  bool exhausted;
  grpc::StatusCode expected;
};

class NoexTableAsyncPollOpImmediateFinishTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<ImmediateFinishTestConfig> {};

TEST_P(NoexTableAsyncPollOpImmediateFinishTest, ImmediateFinish) {
  auto const config = GetParam();
  internal::RPCPolicyParameters const noRetries = {
      std::chrono::hours(0),
      std::chrono::hours(0),
      std::chrono::hours(0),
  };
  auto polling_policy = bigtable::DefaultPollingPolicy(
      config.exhausted ? noRetries : internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  auto async_op_mock = new AsyncOperationMock;
  auto async_op_mock_smart = std::shared_ptr<AsyncOperation>(async_op_mock);

  Functor stored_callback;
  bool op_completed = false;

  auto callback = [&op_completed, config](CompletionQueue&, int& response,
                                          grpc::Status& status) {
    EXPECT_EQ(config.expected, status.error_code());
    EXPECT_EQ(27, response);
    op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(callback), DummyOperation(dummy_op_mock));

  EXPECT_CALL(*dummy_op_mock, Start(_, _, _))
      .WillOnce(
          Invoke([&stored_callback, &async_op_mock_smart](
                     CompletionQueue&, std::unique_ptr<grpc::ClientContext>&,
                     Functor const& callback) {
            stored_callback = callback;
            return async_op_mock_smart;
          }));
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));

  EXPECT_FALSE(stored_callback);
  async_op->Start(cq);
  EXPECT_TRUE(stored_callback);

  grpc::Status status(config.error_code, "");
  stored_callback(cq, config.finished, status);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(op_completed);
}

INSTANTIATE_TEST_CASE_P(
    ImmediateFinish, NoexTableAsyncPollOpImmediateFinishTest,
    ::testing::Values(
        // Finished in first shot.
        ImmediateFinishTestConfig{grpc::StatusCode::OK, true, false,
                                  grpc::StatusCode::OK},
        // Finished in first shot, policy exhausted.
        ImmediateFinishTestConfig{grpc::StatusCode::OK, true, true,
                                  grpc::StatusCode::OK},
        // Finished in first shot but returned UNAVAILABLE
        ImmediateFinishTestConfig{grpc::StatusCode::UNAVAILABLE, true, false,
                                  grpc::StatusCode::UNAVAILABLE},
        // Finished in first shot but returned PERMISSION_DENIED
        ImmediateFinishTestConfig{grpc::StatusCode::PERMISSION_DENIED, true,
                                  false, grpc::StatusCode::PERMISSION_DENIED},
        // RPC succeeded, operation not finished, policy exhausted
        ImmediateFinishTestConfig{grpc::StatusCode::OK, false, true,
                                  grpc::StatusCode::UNKNOWN},
        // RPC failed with UNAVAILABLE, policy exhausted
        ImmediateFinishTestConfig{grpc::StatusCode::UNAVAILABLE, false, true,
                                  grpc::StatusCode::UNAVAILABLE},
        // RPC failed with PERMISSION_DENIED, policy exhausted
        ImmediateFinishTestConfig{grpc::StatusCode::PERMISSION_DENIED, false,
                                  true, grpc::StatusCode::PERMISSION_DENIED}));

class OneRetryTestConfig {
 public:
  // Error code returned from the first Start()
  grpc::StatusCode error_code1;
  // finished flag returned from the second Start()
  bool finished1;
  // Error code returned from the second Start()
  grpc::StatusCode error_code2;
  // finished flag returned from the second Start()
  bool finished2;
  grpc::StatusCode expected;
};

class NoexTableAsyncPollOpOneRetryTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<OneRetryTestConfig> {};

TEST_P(NoexTableAsyncPollOpOneRetryTest, OneRetry) {
  auto const config = GetParam();
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  auto async_op_mock = new AsyncOperationMock;
  auto async_op_mock_smart = std::shared_ptr<AsyncOperation>(async_op_mock);

  Functor stored_callback1;
  Functor stored_callback2;
  bool op_completed = false;

  auto callback = [&op_completed, config](CompletionQueue&, int& response,
                                          grpc::Status& status) {
    EXPECT_EQ(config.expected, status.error_code());
    EXPECT_EQ(27, response);
    op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(callback), DummyOperation(dummy_op_mock));

  EXPECT_CALL(*dummy_op_mock, Start(_, _, _))
      .WillOnce(
          Invoke([&stored_callback1, &async_op_mock_smart](
                     CompletionQueue&, std::unique_ptr<grpc::ClientContext>&,
                     Functor const& callback) {
            stored_callback1 = callback;
            return async_op_mock_smart;
          }))
      .WillOnce(
          Invoke([&stored_callback2, &async_op_mock_smart](
                     CompletionQueue&, std::unique_ptr<grpc::ClientContext>&,
                     Functor const& callback) {
            stored_callback2 = callback;
            return async_op_mock_smart;
          }));
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));

  EXPECT_FALSE(stored_callback1);
  async_op->Start(cq);
  EXPECT_TRUE(stored_callback1);

  {
    grpc::Status status(config.error_code1, "");
    stored_callback1(cq, config.finished1, status);
  }

  EXPECT_EQ(1U, cq_impl->size());  // It's the timer
  EXPECT_FALSE(op_completed);

  cq_impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(stored_callback2);
  EXPECT_FALSE(op_completed);

  {
    grpc::Status status(config.error_code2, "");
    stored_callback2(cq, config.finished2, status);
  }

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(op_completed);
}

INSTANTIATE_TEST_CASE_P(
    OneRetry, NoexTableAsyncPollOpOneRetryTest,
    ::testing::Values(
        // No error, not finished, then OK, finished == true,
        // return OK
        OneRetryTestConfig{grpc::StatusCode::OK, false, grpc::StatusCode::OK,
                           true, grpc::StatusCode::OK},
        // Transient error, then OK, finished == true,
        // return OK
        OneRetryTestConfig{grpc::StatusCode::UNAVAILABLE, false,
                           grpc::StatusCode::OK, true, grpc::StatusCode::OK},
        // Transient error, then still transient from
        // underlying op, finished == true,
        // return UNAVAILABLE
        OneRetryTestConfig{grpc::StatusCode::UNAVAILABLE, false,
                           grpc::StatusCode::UNAVAILABLE, true,
                           grpc::StatusCode::UNAVAILABLE}));

TEST_F(NoexTableAsyncPollOpTest, CancelBeforeStart) {
  // Test if calling `Start` on an already cancelled operation completes the
  // operation immediately with a `CANCELLED` status.
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  // Will be satisfied when the operation completed for the user's point of
  // view.
  std::promise<void> user_op_completed;

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  auto callback = [&user_op_completed](CompletionQueue&, int& response,
                                       grpc::Status& status) {
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    EXPECT_EQ(27, response);
    user_op_completed.set_value();
  };
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(callback), DummyOperation(dummy_op_mock));

  async_op->Cancel();
  async_op->Start(cq);

  user_op_completed.get_future().get();

  cq.Shutdown();
  pool.join();
}

struct CancelInOpTestConfig {
  grpc::StatusCode dummy_op_finish_code;
  bool poll_finished;
  bool poll_exhausted;
  grpc::StatusCode expected;
};

class NoexTableAsyncPollOpCancelInOpTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<CancelInOpTestConfig> {};

TEST_P(NoexTableAsyncPollOpCancelInOpTest, CancelInOperation) {
  CancelInOpTestConfig const config = GetParam();

  // In order to simulate polling_policy.Exhausted() we create a policy which
  // doesn't allow any retries (as specified by `noRetries`). We use it when
  // config.poll_exhausted is true.
  internal::RPCPolicyParameters const noRetries = {
      std::chrono::hours(0),
      std::chrono::hours(0),
      std::chrono::hours(0),
  };
  auto polling_policy = bigtable::DefaultPollingPolicy(
      config.poll_exhausted ? noRetries : internal::kBigtableLimits);

  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  // The underlying, polled operation.
  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  // This is the handle which the user receives from DummyOperation::Start().
  auto dummy_op_handle_mock =
      std::shared_ptr<AsyncOperation>(new AsyncOperationMock);

  // This variable will hold the callback passed by the AsyncPollOp to
  // DummyOperation::Start(), which it wants to be called once DummyOperation
  // completes.
  Functor on_dummy_op_finished;
  bool cancel_called = false;
  // Will be set to true when the operation completed for the user's point of
  // view.
  bool user_op_completed = false;

  auto user_callback = [&user_op_completed, config](CompletionQueue&,
                                                    int& response,
                                                    grpc::Status& status) {
    EXPECT_EQ(status.error_code(), config.expected);
    EXPECT_EQ(27, response);
    user_op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(user_callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(user_callback), DummyOperation(dummy_op_mock));

  EXPECT_CALL(*dummy_op_mock, Start(_, _, _))
      .WillOnce(
          Invoke([&on_dummy_op_finished, &dummy_op_handle_mock](
                     CompletionQueue&, std::unique_ptr<grpc::ClientContext>&,
                     Functor const& callback) {
            on_dummy_op_finished = callback;
            return dummy_op_handle_mock;
          }));
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));
  EXPECT_CALL(static_cast<AsyncOperationMock&>(*dummy_op_handle_mock), Cancel())
      .WillOnce(Invoke([&cancel_called]() { cancel_called = true; }));

  EXPECT_FALSE(on_dummy_op_finished);
  EXPECT_FALSE(cancel_called);
  async_op->Start(cq);
  EXPECT_TRUE(on_dummy_op_finished);
  EXPECT_FALSE(cancel_called);
  EXPECT_TRUE(cq_impl->empty());

  // Now we're in the middle of the simulated operation. We'll finish it by
  // calling on_dummy_op_finished. Before we finish the operation, we'll cancel
  // it though.

  async_op->Cancel();
  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(cancel_called);

  grpc::Status status(config.dummy_op_finish_code, "");
  on_dummy_op_finished(cq, config.poll_finished, status);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_CASE_P(
    CancelInOperation, NoexTableAsyncPollOpCancelInOpTest,
    ::testing::Values(
        // Simulate Cancel being called when an underlying operation is ongoing.
        // We assume that the underlying operation handled it and returned
        // CANCELLED. In such a scenario, CANCELLED status should be reported.
        // The `poll_finished` parameter shouldn't affect the result (though it
        // doesn't make much sense for it to be false, TBH).
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::CANCELLED,
            .poll_finished = true,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::CANCELLED},
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::CANCELLED,
            .poll_finished = false,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::CANCELLED},
        // Simulate Cancel call happening exactly between an underlying
        // operation succeeding and its callback being called. If the operation
        // is finished, it should return OK, otherwise - CANCELLED.
        CancelInOpTestConfig{.dummy_op_finish_code = grpc::StatusCode::OK,
                             .poll_finished = true,
                             .poll_exhausted = false,
                             .expected = grpc::StatusCode::OK},
        CancelInOpTestConfig{.dummy_op_finish_code = grpc::StatusCode::OK,
                             .poll_finished = false,
                             .poll_exhausted = false,
                             .expected = grpc::StatusCode::CANCELLED},
        // Just like the above case, except an error has been reported.
        // If the operation is finished, we should return it, otherwise,
        // CANCELLED.
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::UNAVAILABLE,
            .poll_finished = true,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::UNAVAILABLE},
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::UNAVAILABLE,
            .poll_finished = false,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::CANCELLED},
        // Just like the above case, except the polling policy says it's not
        // retriable, so we return the original error as if there was no cancel.
        // This should happen for both poll_finished and not.
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::PERMISSION_DENIED,
            .poll_finished = true,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::PERMISSION_DENIED},
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::PERMISSION_DENIED,
            .poll_finished = false,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::PERMISSION_DENIED},
        // Just like the above UNAVAILABLE case, except, the polling policy says
        // that we've exhausted the retries.
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::UNAVAILABLE,
            .poll_finished = true,
            .poll_exhausted = true,
            .expected = grpc::StatusCode::UNAVAILABLE},
        CancelInOpTestConfig{
            .dummy_op_finish_code = grpc::StatusCode::UNAVAILABLE,
            .poll_finished = false,
            .poll_exhausted = true,
            .expected = grpc::StatusCode::UNAVAILABLE}));

// This test checks if the Cancel request is propagated to the actual timer.
// Because it is hard to mock it, it runs an actual CompletionQueue.
TEST_F(NoexTableAsyncPollOpTest, TestRealTimerCancellation) {
  // In order to make sure that the timer scheduled by AsyncPollOp doesn't
  // expire during the test, we need to increase the delay in PollingPolicy.
  internal::RPCPolicyParameters const inifinitePoll = {
      std::chrono::hours(100),
      std::chrono::hours(1000),
      std::chrono::hours(10000),
  };
  auto polling_policy = bigtable::DefaultPollingPolicy(inifinitePoll);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();

  // This variable will hold the callback passed by the AsyncPollOp to
  // DummyOperation::Start(), which it wants to be run once DummyOperation
  // completes.
  Functor on_dummy_op_finished;
  std::promise<void> user_op_completed_promise;

  auto user_callback = [&user_op_completed_promise](CompletionQueue&,
                                                    int& response,
                                                    grpc::Status& status) {
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(27, response);
    user_op_completed_promise.set_value();
  };

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(user_callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(user_callback), DummyOperation(dummy_op_mock));

  EXPECT_CALL(*dummy_op_mock, Start(_, _, _))
      .WillOnce(
          Invoke([&on_dummy_op_finished](CompletionQueue&,
                                         std::unique_ptr<grpc::ClientContext>&,
                                         Functor const& callback) {
            on_dummy_op_finished = callback;
            return std::shared_ptr<AsyncOperation>(new AsyncOperationMock);
          }));
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));

  EXPECT_FALSE(on_dummy_op_finished);
  async_op->Start(cq);
  EXPECT_TRUE(on_dummy_op_finished);

  // Now we're in the middle of the simulated operation. We'll finish it by
  // calling on_dummy_op_finished. We're executing the AsyncPollOp code
  // synchronously here, so we can be sure that the timer has been scheduled
  // after the following call returns.

  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "");
  on_dummy_op_finished(cq, false, status);

  auto user_op_completed_future = user_op_completed_promise.get_future();
  // The whole operation should not complete yet.
  EXPECT_EQ(std::future_status::timeout,
            user_op_completed_future.wait_for(50_ms));

  // We're now reasonably sure that the timer hasn't completed, so we can test
  // its cancellation.
  async_op->Cancel();

  user_op_completed_future.get();

  cq.Shutdown();
  pool.join();
}

class NoexTableAsyncPollOpCancelInTimerTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<bool> {};

TEST_P(NoexTableAsyncPollOpCancelInTimerTest, TestCancelInTimer) {
  bool const notice_cancel = GetParam();

  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();

  // This variable will hold the callback passed by the AsyncPollOp to
  // DummyOperation, which it wants to be run once DummyOperation completes.
  Functor on_dummy_op_finished;
  // Will be set to true when the operation completed for the user's point of
  // view.
  bool user_op_completed = false;

  auto user_callback = [&user_op_completed](CompletionQueue&, int& response,
                                            grpc::Status& status) {
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    EXPECT_EQ(27, response);
    user_op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(user_callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(user_callback), DummyOperation(dummy_op_mock));

  EXPECT_CALL(*dummy_op_mock, Start(_, _, _))
      .WillOnce(
          Invoke([&on_dummy_op_finished](CompletionQueue&,
                                         std::unique_ptr<grpc::ClientContext>&,
                                         Functor const& callback) {
            on_dummy_op_finished = callback;
            return std::shared_ptr<AsyncOperation>(new AsyncOperationMock);
          }));
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));

  EXPECT_FALSE(on_dummy_op_finished);
  async_op->Start(cq);
  EXPECT_TRUE(on_dummy_op_finished);

  // Now we're in the middle of the simulated operation. We'll finish it by
  // calling on_dummy_op_finished with a failure.
  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "");
  on_dummy_op_finished(cq, false, status);

  // Now a timer should have been scheduled.
  EXPECT_EQ(1U, cq_impl->size());
  EXPECT_FALSE(user_op_completed);

  // Let's call Cancel on the timer (will be a noop on a mock queue).
  async_op->Cancel();
  EXPECT_EQ(1U, cq_impl->size());

  // Sometime the timer might return timeout despite having been cancelled.
  cq_impl->SimulateCompletion(cq, not notice_cancel);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_CASE_P(
    CancelInTimer, NoexTableAsyncPollOpCancelInTimerTest,
    ::testing::Values(
        // Simulate Cancel being called when sleeping in a timer.
        // This test checks the case when the timer noticed and reported
        // CANCELLED status.
        true,
        // Similar scenario, except we test the corner case in which the Cancel
        // request happens exactly between the timer timing out and a callback
        // being fired. In this scenario the timer reports an OK status, but we
        // should still return CANCELLED to the user.
        false));

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
