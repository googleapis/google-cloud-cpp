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
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>
#include <future>
#include <thread>
#include <typeinfo>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::v2;
using namespace ::testing;
using namespace google::cloud::testing_util::chrono_literals;

class BackoffPolicyMock : public bigtable::RPCBackoffPolicy {
 public:
  BackoffPolicyMock() : num_calls_from_last_clone_() {}

  MOCK_METHOD1(OnCompletionHook, std::chrono::milliseconds(Status const& s));

  std::chrono::milliseconds OnCompletion(grpc::Status const& s) override {
    num_calls_from_last_clone_++;
    return OnCompletionHook(MakeStatusFromRpcError(s));
  }

  std::chrono::milliseconds OnCompletion(
      google::cloud::Status const& s) override {
    num_calls_from_last_clone_++;
    return OnCompletionHook(s);
  }

  std::unique_ptr<RPCBackoffPolicy> clone() const override {
    return std::unique_ptr<RPCBackoffPolicy>();
  }

  void Setup(grpc::ClientContext&) const override {}

  int num_calls_from_last_clone_;
};

// Pretend independent backoff policies, but be only one under the hood.
class SharedBackoffPolicyMock : public bigtable::RPCBackoffPolicy {
 public:
  SharedBackoffPolicyMock() : state_(new BackoffPolicyMock) {}

  std::chrono::milliseconds OnCompletion(grpc::Status const& s) override {
    return state_->OnCompletion(s);
  }

  std::chrono::milliseconds OnCompletion(
      google::cloud::Status const& s) override {
    return state_->OnCompletion(s);
  }

  std::unique_ptr<RPCBackoffPolicy> clone() const override {
    state_->num_calls_from_last_clone_ = 0;
    return std::unique_ptr<RPCBackoffPolicy>(
        new SharedBackoffPolicyMock(*this));
  }

  void Setup(grpc::ClientContext&) const override {}

  int NumCallsFromLastClone() { return state_->num_calls_from_last_clone_; }

  std::shared_ptr<BackoffPolicyMock> state_;
};

using google::bigtable::admin::v2::Cluster;
using google::bigtable::admin::v2::ListClustersRequest;
using google::bigtable::admin::v2::ListClustersResponse;

class AsyncMultipageFutureTest : public ::testing::Test {
 public:
  AsyncMultipageFutureTest()
      : rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        shared_backoff_policy_mock_(
            google::cloud::internal::make_unique<SharedBackoffPolicyMock>()),
        cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new testing::MockInstanceAdminClient),
        metadata_update_policy_("my_instance", MetadataParamTypes::NAME) {}

  // Description of a single expected RPC exchange.
  struct Exchange {
    // The mock will return this status
    grpc::StatusCode status_code;
    // The mock will return these clusters.
    std::vector<std::string> clusters;
    // The mock will return this next_page_token.
    std::string next_page_token;
  };

  using MockAsyncListClustersReader =
      google::cloud::bigtable::testing::MockAsyncResponseReader<
          ListClustersResponse>;

  void ExpectInteraction(std::vector<Exchange> const& interaction) {
    for (auto exchange_it = interaction.rbegin();
         exchange_it != interaction.rend(); ++exchange_it) {
      auto const& exchange = *exchange_it;
      auto cluster_reader = new MockAsyncListClustersReader();
      readers_to_delete_.emplace_back(cluster_reader);

      // We expect the token to be the same as returned by last exchange
      // returning success.
      auto last_success = std::find_if(
          exchange_it + 1, interaction.rend(), [](Exchange const& e) {
            return e.status_code == grpc::StatusCode::OK;
          });
      std::string expected_token = (last_success == interaction.rend())
                                       ? std::string()
                                       : last_success->next_page_token;

      EXPECT_CALL(*client_, AsyncListClusters(_, _, _))
          .WillOnce(Invoke([cluster_reader, expected_token](
                               grpc::ClientContext*,
                               ListClustersRequest const& request,
                               grpc::CompletionQueue*) {
            EXPECT_EQ(expected_token, request.page_token());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<
                grpc::ClientAsyncResponseReaderInterface<ListClustersResponse>>(
                cluster_reader);
          }))
          .RetiresOnSaturation();
      EXPECT_CALL(*cluster_reader, Finish(_, _, _))
          .WillOnce(Invoke([exchange](ListClustersResponse* response,
                                      grpc::Status* status, void*) {
            for (auto const& cluster_name : exchange.clusters) {
              auto& cluster = *response->add_clusters();
              cluster.set_name(cluster_name);
            }
            // Return the right token.
            response->set_next_page_token(exchange.next_page_token);
            *status = grpc::Status(exchange.status_code, "");
          }));
    }
  }

  future<StatusOr<std::vector<std::string>>> StartOp() {
    return StartAsyncRetryMultiPage(
        __func__, std::move(rpc_retry_policy_),
        shared_backoff_policy_mock_->clone(),
        std::move(metadata_update_policy_),
        [this](grpc::ClientContext* context, ListClustersRequest const& request,
               grpc::CompletionQueue* cq) {
          return client_->AsyncListClusters(context, request, cq);
        },
        ListClustersRequest(), std::vector<std::string>(),
        [](std::vector<std::string> accumulator,
           ListClustersResponse response) {
          std::transform(response.clusters().begin(), response.clusters().end(),
                         std::back_inserter(accumulator),
                         [](Cluster const& cluster) { return cluster.name(); });
          return accumulator;
        },
        cq_);
  }

 protected:
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<SharedBackoffPolicyMock> shared_backoff_policy_mock_;
  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<testing::MockInstanceAdminClient> client_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::vector<std::unique_ptr<MockAsyncListClustersReader>> readers_to_delete_;
};

TEST_F(AsyncMultipageFutureTest, ImmediateSuccess) {
  ExpectInteraction({{grpc::StatusCode::OK, {"cluster_1"}, ""}});

  future<StatusOr<std::vector<std::string>>> clusters_future = StartOp();
  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto clusters = clusters_future.get();
  ASSERT_STATUS_OK(clusters);
  std::vector<std::string> const expected_clusters{"cluster_1"};
}

TEST_F(AsyncMultipageFutureTest, NoDelayBetweenSuccesses) {
  ExpectInteraction({{grpc::StatusCode::OK, {"cluster_1"}, "token_1"},
                     {grpc::StatusCode::OK, {"cluster_2"}, ""}});

  future<StatusOr<std::vector<std::string>>> clusters_future = StartOp();
  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto clusters = clusters_future.get();
  ASSERT_STATUS_OK(clusters);
  std::vector<std::string> const expected_clusters{
      "cluster_1",
      "cluster_2",
  };
  EXPECT_EQ(expected_clusters, *clusters);
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncMultipageFutureTest, DelayGrowsOnFailures) {
  ExpectInteraction({{grpc::StatusCode::UNAVAILABLE, {}, ""},
                     {grpc::StatusCode::UNAVAILABLE, {}, ""},
                     {grpc::StatusCode::OK, {"cluster_1"}, ""}});
  EXPECT_CALL(*shared_backoff_policy_mock_->state_, OnCompletionHook(_))
      .WillOnce(Return(std::chrono::milliseconds(1)))
      .WillOnce(Return(std::chrono::milliseconds(1)));

  future<StatusOr<std::vector<std::string>>> clusters_future = StartOp();
  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(1, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer
  EXPECT_EQ(1, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(2, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer
  EXPECT_EQ(2, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto clusters = clusters_future.get();
  ASSERT_STATUS_OK(clusters);
  std::vector<std::string> const expected_clusters{"cluster_1"};
  EXPECT_EQ(expected_clusters, *clusters);
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncMultipageFutureTest, SucessResetsBackoffPolicy) {
  ExpectInteraction({{grpc::StatusCode::UNAVAILABLE, {}, ""},
                     {grpc::StatusCode::OK, {"cluster_1"}, "token1"},
                     {grpc::StatusCode::UNAVAILABLE, {}, ""},
                     {grpc::StatusCode::OK, {"cluster_2"}, ""}});
  EXPECT_CALL(*shared_backoff_policy_mock_->state_, OnCompletionHook(_))
      .WillOnce(Return(std::chrono::milliseconds(1)))
      .WillOnce(Return(std::chrono::milliseconds(1)));

  future<StatusOr<std::vector<std::string>>> clusters_future = StartOp();
  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(1, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer
  EXPECT_EQ(1, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(0, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(1, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer
  EXPECT_EQ(1, shared_backoff_policy_mock_->NumCallsFromLastClone());

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(0, shared_backoff_policy_mock_->NumCallsFromLastClone());

  auto clusters = clusters_future.get();
  ASSERT_STATUS_OK(clusters);
  std::vector<std::string> const expected_clusters{"cluster_1", "cluster_2"};
  EXPECT_EQ(expected_clusters, *clusters);
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncMultipageFutureTest, TransientErrorsAreRetried) {
  ExpectInteraction({{grpc::StatusCode::UNAVAILABLE, {}, ""},
                     {grpc::StatusCode::OK, {"cluster_1"}, ""}});
  EXPECT_CALL(*shared_backoff_policy_mock_->state_, OnCompletionHook(_))
      .WillOnce(Return(std::chrono::milliseconds(1)));

  future<StatusOr<std::vector<std::string>>> clusters_future = StartOp();
  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer

  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto clusters = clusters_future.get();
  ASSERT_STATUS_OK(clusters);
  std::vector<std::string> const expected_clusters{"cluster_1"};
  EXPECT_EQ(expected_clusters, *clusters);
  EXPECT_TRUE(cq_impl_->empty());
}

TEST_F(AsyncMultipageFutureTest, PermanentErrorsAreNotRetried) {
  ExpectInteraction({{grpc::StatusCode::PERMISSION_DENIED, {}, ""}});

  future<StatusOr<std::vector<std::string>>> clusters_future = StartOp();
  EXPECT_EQ(clusters_future.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto clusters = clusters_future.get();
  ASSERT_FALSE(clusters);
  EXPECT_EQ(StatusCode::kPermissionDenied, clusters.status().code());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
