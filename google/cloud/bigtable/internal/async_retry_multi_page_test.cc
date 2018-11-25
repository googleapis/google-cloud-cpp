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

#include "google/cloud/bigtable/internal/async_retry_multi_page.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <future>
#include <thread>
#include <typeinfo>

namespace std {
namespace chrono {

void PrintTo(std::chrono::milliseconds const& ms, std::ostream* stream) {
  *stream << ms.count() << "ms";
}

void PrintTo(std::chrono::seconds const& s, std::ostream* stream) {
  *stream << s.count() << "s";
}

}  // namespace chrono
}  // namespace std

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;
using google::cloud::internal::make_unique;

class RetryMultiPageTest
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

class MultipagePollingPolicyTest
    : public bigtable::testing::internal::TableTestFixture {
 public:
  MultipagePollingPolicyTest()
      : rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy_(
            bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits)),
        polling_policy_(make_unique<MultipagePollingPolicy>(
            rpc_retry_policy_->clone(), rpc_backoff_policy_->clone())) {}

 protected:
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::unique_ptr<PollingPolicy> polling_policy_;
};

TEST_F(MultipagePollingPolicyTest, NoDelayBetweenPages) {
  EXPECT_TRUE(polling_policy_->OnFailure(grpc::Status()));
  EXPECT_EQ(0_s, polling_policy_->WaitPeriod());
  EXPECT_TRUE(polling_policy_->OnFailure(grpc::Status()));
  EXPECT_EQ(0_s, polling_policy_->WaitPeriod());
  EXPECT_TRUE(polling_policy_->OnFailure(grpc::Status()));
  EXPECT_EQ(0_s, polling_policy_->WaitPeriod());
}

TEST_F(MultipagePollingPolicyTest, DelayGrowsOnFailures) {
  grpc::Status failed(grpc::StatusCode::UNAVAILABLE, "oh no");
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  auto wait_period1 = polling_policy_->WaitPeriod();
  EXPECT_LT(0_s, wait_period1);
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  auto wait_period2 = polling_policy_->WaitPeriod();
  EXPECT_LT(wait_period1, wait_period2);
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  EXPECT_LT(wait_period2, polling_policy_->WaitPeriod());
}

TEST_F(MultipagePollingPolicyTest, OnFailureSuccessResetsBackoff) {
  grpc::Status failed(grpc::StatusCode::UNAVAILABLE, "oh no");
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  EXPECT_LT(0_s, polling_policy_->WaitPeriod());
  EXPECT_TRUE(polling_policy_->OnFailure(grpc::Status()));
  EXPECT_EQ(0_s, polling_policy_->WaitPeriod());
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  // Make sure that the backoff policy is reset too.
  EXPECT_GE(kBigtableLimits.initial_delay, polling_policy_->WaitPeriod());
}

TEST_F(MultipagePollingPolicyTest, OnFailureSuccessEliminatesDelay) {
  grpc::Status failed(grpc::StatusCode::UNAVAILABLE, "oh no");
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  EXPECT_LT(0_s, polling_policy_->WaitPeriod());
  EXPECT_TRUE(polling_policy_->OnFailure(grpc::Status()));
  EXPECT_EQ(0_s, polling_policy_->WaitPeriod());
}

TEST_F(MultipagePollingPolicyTest, ExhaustedDoesntResetBackoff) {
  grpc::Status failed(grpc::StatusCode::UNAVAILABLE, "oh no");
  EXPECT_TRUE(polling_policy_->OnFailure(failed));
  EXPECT_LT(0_s, polling_policy_->WaitPeriod());
  EXPECT_FALSE(polling_policy_->Exhausted());
  EXPECT_LT(0_s, polling_policy_->WaitPeriod());
}

class MultipageOneRetryTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<bool> {};

TEST_P(MultipageOneRetryTest, OneRetry) {
  bool const inject_error = GetParam();
  auto rpc_retry_policy =
      bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits);
  auto rpc_backoff_policy =
      bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kTableId,
                                              MetadataParamTypes::TABLE_NAME);

  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  CompletionQueue cq(cq_impl);

  auto dummy_op_mock = std::make_shared<DummyOperationMock>();

  Functor on_dummy_op1_finished;
  Functor on_dummy_op2_finished;
  bool user_op_completed = false;

  auto user_callback = [&user_op_completed](CompletionQueue&, int& response,
                                            grpc::Status& status) {
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(27, response);
    user_op_completed = true;
  };

  auto async_op = std::make_shared<
      internal::AsyncRetryMultiPage<decltype(user_callback), DummyOperation>>(
      __func__, rpc_retry_policy->clone(), rpc_backoff_policy->clone(),
      metadata_update_policy, std::move(user_callback),
      DummyOperation(dummy_op_mock));

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
    grpc::Status status(
        inject_error ? grpc::StatusCode::UNAVAILABLE : grpc::StatusCode::OK,
        "");
    on_dummy_op1_finished(cq, false, status);
  }

  if (inject_error) {
    EXPECT_EQ(1U, cq_impl->size());  // It's the timer
    EXPECT_FALSE(user_op_completed);
    EXPECT_FALSE(on_dummy_op2_finished);
    cq_impl->SimulateCompletion(cq, true);
  }

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(on_dummy_op2_finished);
  EXPECT_FALSE(user_op_completed);

  {
    grpc::Status status;
    on_dummy_op2_finished(cq, true, status);
  }

  EXPECT_TRUE(cq_impl->empty());
  EXPECT_TRUE(user_op_completed);
}

INSTANTIATE_TEST_CASE_P(
    OneRetry, MultipageOneRetryTest,
    ::testing::Values(
        // Make fetching the first bit of data fail and make the second be the
        // only piece of data.
        true,
        // Make the data come in two pieces, both returning OK.
        false));

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
