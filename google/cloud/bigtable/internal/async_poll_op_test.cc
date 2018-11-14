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

  void Cancel() override {
    google::cloud::internal::RaiseLogicError(
        "Cancellation is not yet supported in AsyncPollOp");
  }
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

INSTANTIATE_TEST_CASE_P(
    ImmediateFinish, NoexTableAsyncPollOpImmediateFinishTest,
    ::testing::Values(
        // Finished in first shot.
        ImmediateFinishTestConfig{.dummy_op_error_code = grpc::StatusCode::OK,
                                  .dummy_op_poll_finished = true,
                                  .poll_exhausted = false,
                                  .expected = grpc::StatusCode::OK},
        // Finished in first shot, policy exhausted.
        ImmediateFinishTestConfig{.dummy_op_error_code = grpc::StatusCode::OK,
                                  .dummy_op_poll_finished = true,
                                  .poll_exhausted = true,
                                  .expected = grpc::StatusCode::OK},
        // Finished in first shot but returned UNAVAILABLE
        ImmediateFinishTestConfig{
            .dummy_op_error_code = grpc::StatusCode::UNAVAILABLE,
            .dummy_op_poll_finished = true,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::UNAVAILABLE},
        // Finished in first shot but returned PERMISSION_DENIED
        ImmediateFinishTestConfig{
            .dummy_op_error_code = grpc::StatusCode::PERMISSION_DENIED,
            .dummy_op_poll_finished = true,
            .poll_exhausted = false,
            .expected = grpc::StatusCode::PERMISSION_DENIED},
        // RPC succeeded, operation not finished, policy exhausted
        ImmediateFinishTestConfig{.dummy_op_error_code = grpc::StatusCode::OK,
                                  .dummy_op_poll_finished = false,
                                  .poll_exhausted = true,
                                  .expected = grpc::StatusCode::UNKNOWN},
        // RPC failed with UNAVAILABLE, policy exhausted
        ImmediateFinishTestConfig{
            .dummy_op_error_code = grpc::StatusCode::UNAVAILABLE,
            .dummy_op_poll_finished = false,
            .poll_exhausted = true,
            .expected = grpc::StatusCode::UNAVAILABLE},
        // RPC failed with PERMISSION_DENIED, policy exhausted
        ImmediateFinishTestConfig{
            .dummy_op_error_code = grpc::StatusCode::PERMISSION_DENIED,
            .dummy_op_poll_finished = false,
            .poll_exhausted = true,
            .expected = grpc::StatusCode::PERMISSION_DENIED}));

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

INSTANTIATE_TEST_CASE_P(
    OneRetry, NoexTableAsyncPollOpOneRetryTest,
    ::testing::Values(
        // No error, not finished, then OK, finished == true,
        // return OK
        OneRetryTestConfig{.dummy_op1_error_code = grpc::StatusCode::OK,
                           .dummy_op1_poll_finished = false,
                           .dummy_op2_error_code = grpc::StatusCode::OK,
                           .dummy_op2_poll_finished = true,
                           .expected = grpc::StatusCode::OK},
        // Transient error, then OK, finished == true,
        // return OK
        OneRetryTestConfig{
            .dummy_op1_error_code = grpc::StatusCode::UNAVAILABLE,
            .dummy_op1_poll_finished = false,
            .dummy_op2_error_code = grpc::StatusCode::OK,
            .dummy_op2_poll_finished = true,
            .expected = grpc::StatusCode::OK},
        // Transient error, then still transient from
        // underlying op, finished == true,
        // return UNAVAILABLE
        OneRetryTestConfig{
            .dummy_op1_error_code = grpc::StatusCode::UNAVAILABLE,
            .dummy_op1_poll_finished = false,
            .dummy_op2_error_code = grpc::StatusCode::UNAVAILABLE,
            .dummy_op2_poll_finished = true,
            .expected = grpc::StatusCode::UNAVAILABLE}));

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
