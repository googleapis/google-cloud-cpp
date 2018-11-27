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

class NoexTableAsyncRetryOpTest
    : public bigtable::testing::internal::TableTestFixture {};

using Functor = std::function<void(CompletionQueue&, grpc::Status&)>;

// Operation passed to AsyncRetryOp needs to be movable or copyable. Mocked
// objects are neither, so we'll mock this class and hold a shared_ptr to it in
// DummyOperation. DummyOperation will satisfy the requirements for a parameter
// to AsyncRetryOp.
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

  template <typename F, typename std::enable_if<
                            google::cloud::internal::is_invocable<
                                F, CompletionQueue&, grpc::Status&>::value,
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

TEST_F(NoexTableAsyncRetryOpTest, CancelBeforeStart) {
  // Test if calling `Start` on an already cancelled operation completes the
  // operation immediately with a `CANCELLED` status.
  auto rpc_retry_policy =
      bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits);
  auto rpc_backoff_policy =
      bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  std::promise<void> user_op_completed;
  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  auto user_callback = [&user_op_completed](CompletionQueue&, int& response,
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
      internal::AsyncRetryOp<internal::ConstantIdempotencyPolicy,
                             decltype(user_callback), DummyOperation>>(
      __func__, rpc_retry_policy->clone(), rpc_backoff_policy->clone(),
      internal::ConstantIdempotencyPolicy(true), metadata_update_policy,
      std::move(user_callback), DummyOperation(dummy_op_mock));

  async_op->Cancel();
  async_op->Start(cq);

  user_op_completed.get_future().get();

  cq.Shutdown();
  pool.join();
}

struct CancelInOpTestConfig {
  grpc::StatusCode dummy_op_error_code;
  bool idempotent;
  grpc::StatusCode expected;
};

class NoexTableAsyncRetryOpCancelInOpTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<CancelInOpTestConfig> {};

TEST_P(NoexTableAsyncRetryOpCancelInOpTest, CancelInOperation) {
  CancelInOpTestConfig const config = GetParam();

  auto rpc_retry_policy =
      bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits);
  auto rpc_backoff_policy =
      bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  auto dummy_op_handle_mock =
      std::shared_ptr<AsyncOperation>(new AsyncOperationMock);

  Functor on_dummy_op_finished;
  bool cancel_called = false;
  bool user_op_completed = false;

  auto user_callback = [&user_op_completed, config](CompletionQueue&,
                                                    int& response,
                                                    grpc::Status& status) {
    EXPECT_EQ(status.error_code(), config.expected);
    EXPECT_EQ(27, response);
    user_op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncRetryOp<internal::ConstantIdempotencyPolicy,
                             decltype(user_callback), DummyOperation>>(
      __func__, rpc_retry_policy->clone(), rpc_backoff_policy->clone(),
      internal::ConstantIdempotencyPolicy(config.idempotent),
      metadata_update_policy, std::move(user_callback),
      DummyOperation(dummy_op_mock));

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

  // Now we're in the middle of the simulated operation. We'll finish it by
  // calling on_dummy_op_finished. Before we finish the operation, we'll cancel
  // it though.

  async_op->Cancel();
  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(cancel_called);

  grpc::Status status(config.dummy_op_error_code, "");
  on_dummy_op_finished(cq, status);

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_CASE_P(
    CancelInOperation, NoexTableAsyncRetryOpCancelInOpTest,
    ::testing::Values(
        // Simulate Cancel being called when an underlying operation is ongoing.
        // We assume that the underlying operation handled it and returned
        // CANCELLED. In such a scenario, CANCELLED status should be reported.
        CancelInOpTestConfig{// DummyOperation will finish with this code.
                             grpc::StatusCode::CANCELLED,
                             // Consider DummyOperation idempotent.
                             true,
                             // Expected overall result.
                             grpc::StatusCode::CANCELLED},
        // Simulate Cancel call happening exactly between an underlying
        // operation succeeding and its callback being called. In such a
        // scenario, a success should be reported.
        CancelInOpTestConfig{// DummyOperation will finish with this code.
                             grpc::StatusCode::OK,
                             // Consider DummyOperation idempotent.
                             true,
                             // Expected overall result.
                             grpc::StatusCode::OK},
        // Just like the above case, except an error has been reported. In such
        // a scenario, we should not retry and return CANCELLED
        CancelInOpTestConfig{// DummyOperation will finish with this code.
                             grpc::StatusCode::UNAVAILABLE,
                             // Consider DummyOperation idempotent.
                             true,
                             // Expected overall result.
                             grpc::StatusCode::CANCELLED},
        // Just like the above case, except the retry policy tells it's not
        // retriable, so we return the original error as if there was no cancel.
        CancelInOpTestConfig{// DummyOperation will finish with this code.
                             grpc::StatusCode::PERMISSION_DENIED,
                             // Consider DummyOperation idempotent.
                             true,
                             // Expected overall result.
                             grpc::StatusCode::PERMISSION_DENIED},
        // Just like the above UNAVAILABLE case, except idempotency forbids
        // retries. In such a scenario, we should return the original error.
        CancelInOpTestConfig{// DummyOperation will finish with this code.
                             grpc::StatusCode::UNAVAILABLE,
                             // Consider DummyOperation not idempotent.
                             false,
                             // Expected overall result.
                             grpc::StatusCode::UNAVAILABLE}));

// This test checks if the Cancel request is propagated to the actual timer.
// Because it is hard to mock it, it runs an actual CompletionQueue.
TEST_F(NoexTableAsyncRetryOpTest, TestRealTimerCancellation) {
  // We're using a real CompletionQueue and a real timer, so we need to make
  // sure it doesn't expire during the test.
  internal::RPCPolicyParameters const inifiniteRetry = {
      std::chrono::hours(100),
      std::chrono::hours(1000),
      std::chrono::hours(10000),
  };
  auto rpc_retry_policy = bigtable::DefaultRPCRetryPolicy(inifiniteRetry);
  auto rpc_backoff_policy = bigtable::DefaultRPCBackoffPolicy(inifiniteRetry);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();
  Functor on_dummy_op_finished;
  std::promise<void> completed_promise;
  auto user_callback = [&completed_promise](CompletionQueue&, int& response,
                                            grpc::Status& status) {
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(27, response);
    completed_promise.set_value();
  };

  auto async_op = std::make_shared<
      internal::AsyncRetryOp<internal::ConstantIdempotencyPolicy,
                             decltype(user_callback), DummyOperation>>(
      __func__, rpc_retry_policy->clone(), rpc_backoff_policy->clone(),
      internal::ConstantIdempotencyPolicy(true), metadata_update_policy,
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
  // calling on_dummy_op_finished. We're executing the AsyncRetryOp code
  // synchronously here, so we can be sure that the timer has been scheduled
  // after the following call returns.

  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "");
  on_dummy_op_finished(cq, status);

  auto completed_future = completed_promise.get_future();
  // The whole operation should not complete yet.
  EXPECT_EQ(std::future_status::timeout, completed_future.wait_for(50_ms));

  // Now the timer is scheduled, let's cancel it.
  async_op->Cancel();

  completed_future.get();

  cq.Shutdown();
  pool.join();
}

class NoexTableAsyncRetryOpCancelInTimerTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<bool> {};

TEST_P(NoexTableAsyncRetryOpCancelInTimerTest, TestCancelInTimer) {
  bool const notice_cancel = GetParam();

  auto rpc_retry_policy =
      bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits);
  auto rpc_backoff_policy =
      bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();

  Functor on_dummy_op_finished;
  bool user_op_completed = false;
  auto user_callback = [&user_op_completed](CompletionQueue&, int& response,
                                            grpc::Status& status) {
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    EXPECT_EQ(27, response);
    user_op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncRetryOp<internal::ConstantIdempotencyPolicy,
                             decltype(user_callback), DummyOperation>>(
      __func__, rpc_retry_policy->clone(), rpc_backoff_policy->clone(),
      internal::ConstantIdempotencyPolicy(true), metadata_update_policy,
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
  on_dummy_op_finished(cq, status);

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
    CancelInTimer, NoexTableAsyncRetryOpCancelInTimerTest,
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
