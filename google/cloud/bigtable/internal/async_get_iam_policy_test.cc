// Copyright 2018 Google LLC.
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

#include "google/cloud/bigtable/internal/async_get_iam_policy.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/internal/instance_admin.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
namespace {

namespace bt = ::google::cloud::bigtable;
namespace btadmin = google::bigtable::admin::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;
using MockAsyncGetIamPolicyReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        google::iam::v1::Policy>;

using Functor = std::function<void(CompletionQueue&, google::cloud::IamPolicy&,
                                   grpc::Status&)>;

class NoexAsyncGetamPolicyTest : public ::testing::Test {
 public:
  NoexAsyncGetamPolicyTest()
      : rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy_(
            bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits)),
        cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new testing::MockInstanceAdminClient),
        user_op_called_(),
        metadata_update_policy_("my_instance", MetadataParamTypes::NAME),
        op_(new internal::AsyncRetryGetIamPolicy<Functor>(
            __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
            metadata_update_policy_, client_, "my-project", "my_instance",
            [this](CompletionQueue& cq, google::cloud::IamPolicy& res,
                   grpc::Status& status) {
              OnUserCompleted(cq, res, status);
            })),
        get_iam_policy_reader_(new MockAsyncGetIamPolicyReader) {}

 protected:
  void OnUserCompleted(CompletionQueue&, google::cloud::IamPolicy& res,
                       grpc::Status& status) {
    user_op_called_ = true;
    user_status_ = status;
    user_res_ = res;
  }

  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<testing::MockInstanceAdminClient> client_;
  grpc::Status user_status_;
  google::cloud::IamPolicy user_res_;
  bool user_op_called_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<internal::AsyncRetryGetIamPolicy<Functor>> op_;
  std::unique_ptr<MockAsyncGetIamPolicyReader> get_iam_policy_reader_;
};

/// @test Verify that InstanceAdmin::AsyncGetIamPolicy() works in a simplest
/// case.
TEST_F(NoexAsyncGetamPolicyTest, Simple) {
  EXPECT_CALL(*client_, AsyncGetIamPolicy(_, _, _))
      .WillOnce(Invoke([this](
                           grpc::ClientContext*,
                           google::iam::v1::GetIamPolicyRequest const& request,
                           grpc::CompletionQueue*) {
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>(
            get_iam_policy_reader_.get());
      }));

  EXPECT_CALL(*get_iam_policy_reader_, Finish(_, _, _))
      .WillOnce(Invoke(
          [](google::iam::v1::Policy* response, grpc::Status* status, void*) {
            response->set_etag("test_etag");
            *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
          }));

  op_->Start(cq_);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_TRUE(user_op_called_);
  EXPECT_TRUE(user_status_.ok());
  EXPECT_EQ("test_etag", user_res_.etag);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Verify that InstanceAdmin::AsyncGetIamPolicy() works for retry
/// case.
TEST_F(NoexAsyncGetamPolicyTest, Retry) {
  EXPECT_CALL(*client_, AsyncGetIamPolicy(_, _, _))
      .WillOnce(Invoke([this](
                           grpc::ClientContext*,
                           google::iam::v1::GetIamPolicyRequest const& request,
                           grpc::CompletionQueue*) {
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>(
            get_iam_policy_reader_.get());
      }))
      .WillOnce(Invoke([this](
                           grpc::ClientContext*,
                           google::iam::v1::GetIamPolicyRequest const& request,
                           grpc::CompletionQueue*) {
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>(
            get_iam_policy_reader_.get());
      }));

  EXPECT_CALL(*get_iam_policy_reader_, Finish(_, _, _))
      .WillOnce(Invoke(
          [](google::iam::v1::Policy* response, grpc::Status* status, void*) {
            response->set_etag("test_etag");
            *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "");
          }))
      .WillOnce(Invoke(
          [](google::iam::v1::Policy* response, grpc::Status* status, void*) {
            response->set_etag("test_etag");
            *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
          }));

  op_->Start(cq_);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_TRUE(user_op_called_);
  EXPECT_TRUE(user_status_.ok());
  EXPECT_EQ("test_etag", user_res_.etag);
  EXPECT_TRUE(cq_impl_->empty());
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
