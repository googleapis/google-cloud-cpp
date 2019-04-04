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
#include "google/cloud/testing_util/assert_ok.h"
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

  int AccumulatedResult() { return impl_->AccumulatedResult(); }

 private:
  std::shared_ptr<DummyOperationImpl> impl_;
};

class AsyncOperationMock : public AsyncOperation {
 public:
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
  grpc::StatusCode dummy_op_error_code;
  // Whether DummyOperation reports this poll attempt as finished.
  bool dummy_op_poll_finished;
  bool poll_exhausted;
  grpc::StatusCode expected;
};

class NoexTableAsyncPollOpImmediateFinishTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<ImmediateFinishTestConfig> {};

TEST_P(NoexTableAsyncPollOpImmediateFinishTest, ImmediateFinish) {
  auto const config = GetParam();

  // Make sure polling policy doesn't allow for any retries if
  // config.poll_exhausted is set, so that polling_policy.Exhausted() returns
  // false.
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

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();

  Functor on_dummy_op_finished;
  bool user_op_completed = false;

  auto user_callback = [&user_op_completed, config](CompletionQueue&,
                                                    int& response,
                                                    grpc::Status& status) {
    EXPECT_EQ(config.expected, status.error_code());
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

  grpc::Status status(config.dummy_op_error_code, "");
  on_dummy_op_finished(cq, config.dummy_op_poll_finished, status);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_SUITE_P(
    ImmediateFinish, NoexTableAsyncPollOpImmediateFinishTest,
    ::testing::Values(
        // Finished in first shot.
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::OK},
        // Finished in first shot, policy exhausted.
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            true,
            // Expected overall result.
            grpc::StatusCode::OK},
        // Finished in first shot but returned UNAVAILABLE
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::UNAVAILABLE},
        // Finished in first shot but returned PERMISSION_DENIED
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::PERMISSION_DENIED,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::PERMISSION_DENIED},
        // RPC succeeded, operation not finished, policy exhausted
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // DummyOperation will indicate that operation is not finished.
            false,
            // PollingPolicy.Finish() will return this.
            true,
            // Expected overall result.
            grpc::StatusCode::UNKNOWN},
        // RPC failed with UNAVAILABLE, policy exhausted
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // DummyOperation will indicate that operation is not finished.
            false,
            // PollingPolicy.Finish() will return this.
            true,
            // Expected overall result.
            grpc::StatusCode::UNAVAILABLE},
        // RPC failed with PERMISSION_DENIED, policy exhausted
        ImmediateFinishTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::PERMISSION_DENIED,
            // DummyOperation will indicate that operation is not finished.
            false,
            // PollingPolicy.Finish() will return this.
            true,
            // Expected overall result.
            grpc::StatusCode::PERMISSION_DENIED}));

class OneRetryTestConfig {
 public:
  // Error code which the first DummyOperation ends with.
  grpc::StatusCode dummy_op1_error_code;
  // Whether the first DummyOperation indicates if the poll is finished.
  bool dummy_op1_poll_finished;
  // Error code which the first DummyOperation ends with.
  grpc::StatusCode dummy_op2_error_code;
  // Whether the first DummyOperation indicates if the poll is finished.
  bool dummy_op2_poll_finished;
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

  Functor on_dummy_op1_finished;
  Functor on_dummy_op2_finished;
  bool user_op_completed = false;

  auto user_callback = [&user_op_completed, config](CompletionQueue&,
                                                    int& response,
                                                    grpc::Status& status) {
    EXPECT_EQ(config.expected, status.error_code());
    EXPECT_EQ(27, response);
    user_op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncPollOp<decltype(user_callback), DummyOperation>>(
      __func__, polling_policy->clone(), metadata_update_policy,
      std::move(user_callback), DummyOperation(dummy_op_mock));

  EXPECT_CALL(*dummy_op_mock, Start(_, _, _))
      .WillOnce(
          Invoke([&on_dummy_op1_finished](CompletionQueue&,
                                          std::unique_ptr<grpc::ClientContext>&,
                                          Functor const& callback) {
            on_dummy_op1_finished = callback;
            return std::shared_ptr<AsyncOperation>(new AsyncOperationMock);
          }))
      .WillOnce(
          Invoke([&on_dummy_op2_finished](CompletionQueue&,
                                          std::unique_ptr<grpc::ClientContext>&,
                                          Functor const& callback) {
            on_dummy_op2_finished = callback;
            return std::shared_ptr<AsyncOperation>(new AsyncOperationMock);
          }));
  EXPECT_CALL(*dummy_op_mock, AccumulatedResult()).WillOnce(Invoke([]() {
    return 27;
  }));

  EXPECT_FALSE(on_dummy_op1_finished);
  async_op->Start(cq);
  EXPECT_TRUE(on_dummy_op1_finished);

  {
    grpc::Status status(config.dummy_op1_error_code, "");
    on_dummy_op1_finished(cq, config.dummy_op1_poll_finished, status);
  }

  EXPECT_EQ(1U, cq_impl->size());  // It's the timer
  EXPECT_FALSE(user_op_completed);

  cq_impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(on_dummy_op2_finished);
  EXPECT_FALSE(user_op_completed);

  {
    grpc::Status status(config.dummy_op2_error_code, "");
    on_dummy_op2_finished(cq, config.dummy_op2_poll_finished, status);
  }

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_SUITE_P(
    OneRetry, NoexTableAsyncPollOpOneRetryTest,
    ::testing::Values(
        // No error, not finished, then OK, finished == true,
        // return OK
        OneRetryTestConfig{
            // First DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // First DummyOperation will indicate that operation isn't finished.
            false,
            // Second DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // Second DummyOperation will indicate that operation is finished.
            true,
            // Expected overall result.
            grpc::StatusCode::OK},
        // Transient error, then OK, finished == true,
        // return OK
        OneRetryTestConfig{
            // First DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // First DummyOperation will indicate that operation isn't finished.
            false,
            // Second DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // Second DummyOperation will indicate that operation is finished.
            true,
            // Expected overall result.
            grpc::StatusCode::OK},
        // Transient error, then still transient from
        // underlying op, finished == true,
        // return UNAVAILABLE
        OneRetryTestConfig{
            // First DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // First DummyOperation will indicate that operation isn't finished.
            false,
            // Second DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // Second DummyOperation will indicate that operation is finished.
            true,
            // Expected overall result.
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

INSTANTIATE_TEST_SUITE_P(
    CancelInOperation, NoexTableAsyncPollOpCancelInOpTest,
    ::testing::Values(
        // Simulate Cancel being called when an underlying operation is ongoing.
        // We assume that the underlying operation handled it and returned
        // CANCELLED. In such a scenario, CANCELLED status should be reported.
        // The `poll_finished` parameter shouldn't affect the result (though it
        // doesn't make much sense for it to be false, TBH).
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::CANCELLED,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::CANCELLED},
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::CANCELLED,
            // DummyOperation will indicate that operation isn't finished.
            false,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::CANCELLED},
        // Simulate Cancel call happening exactly between an underlying
        // operation succeeding and its callback being called. If the operation
        // is finished, it should return OK, otherwise - CANCELLED.
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::OK},
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::OK,
            // DummyOperation will indicate that operation isn't finished.
            false,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::CANCELLED},
        // Just like the above case, except an error has been reported.
        // If the operation is finished, we should return it, otherwise,
        // CANCELLED.
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::UNAVAILABLE},
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // DummyOperation will indicate that operation isn't finished.
            false,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::CANCELLED},
        // Just like the above case, except the polling policy says it's not
        // retriable, so we return the original error as if there was no cancel.
        // This should happen for both poll_finished and not.
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::PERMISSION_DENIED,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::PERMISSION_DENIED},
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::PERMISSION_DENIED,
            // DummyOperation will indicate that operation isn't finished.
            false,
            // PollingPolicy.Finish() will return this.
            false,
            // Expected overall result.
            grpc::StatusCode::PERMISSION_DENIED},
        // Just like the above UNAVAILABLE case, except, the polling policy says
        // that we've exhausted the retries.
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // DummyOperation will indicate that operation is finished.
            true,
            // PollingPolicy.Finish() will return this.
            true,
            // Expected overall result.
            grpc::StatusCode::UNAVAILABLE},
        CancelInOpTestConfig{
            // DummyOperation will finish with this code.
            grpc::StatusCode::UNAVAILABLE,
            // DummyOperation will indicate that operation isn't finished.
            false,
            // PollingPolicy.Finish() will return this.
            true,
            // Expected overall result.
            grpc::StatusCode::UNAVAILABLE}));

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
  cq_impl->SimulateCompletion(cq, !notice_cancel);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_SUITE_P(
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

template <typename F>
StatusOr<F> MakeStatusOr(F f) {
  return f;
}

class NoexTableAsyncPollOpImmediateFinishFutureTest
    : public bigtable::testing::internal::TableTestFixture {
 public:
  NoexTableAsyncPollOpImmediateFinishFutureTest()
      : cq_impl_(std::make_shared<bigtable::testing::MockCompletionQueue>()),
        cq_(cq_impl_),
        metadata_update_policy_(kTableId, MetadataParamTypes::TABLE_NAME) {}

  future<StatusOr<int>> StartImpl(bool simulate_retries) {
    internal::RPCPolicyParameters const kNoRetries = {
        std::chrono::hours(0),
        std::chrono::hours(0),
        std::chrono::hours(0),
    };

    auto user_future = internal::StartAsyncPollOp(
        __func__,
        bigtable::DefaultPollingPolicy(
            simulate_retries ? internal::kBigtableLimits : kNoRetries),
        std::move(metadata_update_policy_), cq_,
        make_ready_future(
            MakeStatusOr([this](CompletionQueue& cq,
                                std::unique_ptr<grpc::ClientContext> context) {
              return attempt_promise_.get_future();
            })));

    EXPECT_EQ(std::future_status::timeout, user_future.wait_for(1_ms));
    return user_future;
  }

  future<StatusOr<int>> StartWithRetries() { return StartImpl(true); }

  future<StatusOr<int>> StartWithoutRetries() { return StartImpl(false); }

  std::shared_ptr<testing::MockCompletionQueue> cq_impl_;
  CompletionQueue cq_;
  MetadataUpdatePolicy metadata_update_policy_;
  promise<StatusOr<optional<int>>> attempt_promise_;
};

TEST_F(NoexTableAsyncPollOpImmediateFinishFutureTest, ImmediateSuccess) {
  auto user_future = StartWithRetries();
  attempt_promise_.set_value(optional<int>(42));
  auto res = user_future.get();
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(42, *res);
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(NoexTableAsyncPollOpImmediateFinishFutureTest,
       PolicyExhaustedOnSuccess) {
  auto user_future = StartWithoutRetries();
  attempt_promise_.set_value(optional<int>());  // no value
  auto res = user_future.get();
  ASSERT_EQ(StatusCode::kUnknown, res.status().code());
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(NoexTableAsyncPollOpImmediateFinishFutureTest,
       PolicyExhaustedOnFailure) {
  auto user_future = StartWithoutRetries();
  attempt_promise_.set_value(Status(StatusCode::kUnavailable, "oh no"));
  auto res = user_future.get();
  ASSERT_EQ(StatusCode::kUnavailable, res.status().code());
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(NoexTableAsyncPollOpImmediateFinishFutureTest, PermanentFailure) {
  auto user_future = StartWithRetries();
  attempt_promise_.set_value(Status(StatusCode::kPermissionDenied, "oh no"));
  auto res = user_future.get();
  ASSERT_EQ(StatusCode::kPermissionDenied, res.status().code());
  EXPECT_TRUE(cq_impl_->empty());
}

class NoexTableAsyncPollOpOneRetryFutureTest
    : public bigtable::testing::internal::TableTestFixture {
 public:
  NoexTableAsyncPollOpOneRetryFutureTest()
      : cq_impl_(std::make_shared<bigtable::testing::MockCompletionQueue>()),
        cq_(cq_impl_),
        metadata_update_policy_(kTableId, MetadataParamTypes::TABLE_NAME),
        polling_policy_(
            bigtable::DefaultPollingPolicy(internal::kBigtableLimits)),
        user_future_(internal::StartAsyncPollOp(
            __func__, bigtable::DefaultPollingPolicy(internal::kBigtableLimits),
            std::move(metadata_update_policy_), cq_,
            make_ready_future(MakeStatusOr(
                [this](CompletionQueue& cq,
                       std::unique_ptr<grpc::ClientContext> context) {
                  attempt_promises_.emplace_back();
                  return attempt_promises_.back().get_future();
                })))) {
    EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
    EXPECT_EQ(1U, attempt_promises_.size());
  }

  std::shared_ptr<testing::MockCompletionQueue> cq_impl_;
  CompletionQueue cq_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::vector<promise<StatusOr<optional<int>>>> attempt_promises_;
  future<StatusOr<int>> user_future_;
};

TEST_F(NoexTableAsyncPollOpOneRetryFutureTest, PollNotFinishedThenOk) {
  attempt_promises_.back().set_value(optional<int>());
  EXPECT_EQ(1U, cq_impl_->size());  // It's the timer
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));

  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_TRUE(cq_impl_->empty());
  ASSERT_EQ(2U, attempt_promises_.size());
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));

  attempt_promises_.back().set_value(optional<int>(42));

  EXPECT_TRUE(cq_impl_->empty());
  auto res = user_future_.get();
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(42, *res);
}

TEST_F(NoexTableAsyncPollOpOneRetryFutureTest, TransientFailureThenOk) {
  attempt_promises_.back().set_value(Status(StatusCode::kUnavailable, "oh no"));
  EXPECT_EQ(1U, cq_impl_->size());  // It's the timer
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));

  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_TRUE(cq_impl_->empty());
  ASSERT_EQ(2U, attempt_promises_.size());
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));

  attempt_promises_.back().set_value(optional<int>(42));

  EXPECT_TRUE(cq_impl_->empty());
  auto res = user_future_.get();
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(42, *res);
}

TEST(TableAsyncPollOpOneRetryFutureTest, OperationFutureFails) {
  CompletionQueue cq;
  auto user_future = internal::StartAsyncPollOp(
      __func__, bigtable::DefaultPollingPolicy(internal::kBigtableLimits),
      MetadataUpdatePolicy("some_table", MetadataParamTypes::TABLE_NAME), cq,
      make_ready_future(
          StatusOr<std::function<future<StatusOr<optional<int>>>(
              CompletionQueue & cq, std::unique_ptr<grpc::ClientContext>)>>(
              Status(StatusCode::kUnavailable, ""))));

  auto res = user_future.get();
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kUnavailable, res.status().code());
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
