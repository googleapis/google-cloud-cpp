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

#include "google/cloud/bigtable/internal/async_list_clusters.h"
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
namespace btproto = google::bigtable::admin::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;
using MockAsyncListClustersReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        btproto::ListClustersResponse>;

using Functor =
    std::function<void(CompletionQueue&, ClusterList&, grpc::Status&)>;

class NoexAsyncListClustersTest : public ::testing::Test {
 public:
  NoexAsyncListClustersTest()
      : rpc_retry_policy_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy_(
            bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits)),
        cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new testing::MockInstanceAdminClient),
        user_op_called_(),
        metadata_update_policy_("my_instance", MetadataParamTypes::NAME),
        op_(new internal::AsyncRetryListClusters<Functor>(
            __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
            metadata_update_policy_, client_, "my_instance",
            [this](CompletionQueue& cq, ClusterList& res,
                   grpc::Status& status) {
              OnUserCompleted(cq, res, status);
            })),
        clusters_reader_1_(new MockAsyncListClustersReader),
        clusters_reader_2_(new MockAsyncListClustersReader),
        clusters_reader_3_(new MockAsyncListClustersReader) {}

 protected:
  void OnUserCompleted(CompletionQueue&, ClusterList& res,
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
  ClusterList user_res_;
  bool user_op_called_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<internal::AsyncRetryListClusters<Functor>> op_;
  std::unique_ptr<MockAsyncListClustersReader> clusters_reader_1_;
  std::unique_ptr<MockAsyncListClustersReader> clusters_reader_2_;
  std::unique_ptr<MockAsyncListClustersReader> clusters_reader_3_;
};

// A lambda to create lambdas. Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string returned_token, std::vector<std::string> cluster_names,
       std::vector<std::string> failed_locations) {
      return [returned_token, cluster_names, failed_locations](
                 btproto::ListClustersResponse* response, grpc::Status* status,
                 void*) {
        for (auto const& cluster_name : cluster_names) {
          auto& cluster = *response->add_clusters();
          cluster.set_name(cluster_name);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        for (auto const& failed_location : failed_locations) {
          response->add_failed_locations(failed_location);
        }
        *status = grpc::Status::OK;
      };
    };

std::vector<std::string> ClusterNames(ClusterList const& response) {
  std::vector<std::string> res;
  std::transform(response.clusters.begin(), response.clusters.end(),
                 std::back_inserter(res), [](btproto::Cluster const& cluster) {
                   return cluster.name();
                 });
  return res;
}

/// @test One successful page with 1 one cluster.
TEST_F(NoexAsyncListClustersTest, Simple) {
  EXPECT_CALL(*client_, AsyncListClusters(_, _, _))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_1_.get());
      }));
  EXPECT_CALL(*clusters_reader_1_, Finish(_, _, _))
      .WillOnce(Invoke(
          create_list_clusters_lambda("", {"cluster_1"}, {"failed_loc_1"})));

  op_->Start(cq_);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_TRUE(user_op_called_);
  EXPECT_TRUE(user_status_.ok());
  EXPECT_EQ(std::vector<std::string>{"cluster_1"}, ClusterNames(user_res_));
  EXPECT_EQ(std::vector<std::string>{"failed_loc_1"},
            user_res_.failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 3 pages, no failures, multiple clusters and failed locations.
TEST_F(NoexAsyncListClustersTest, MultipleClustersAndLocations) {
  EXPECT_CALL(*client_, AsyncListClusters(_, _, _))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_1_.get());
      }))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_2_.get());
      }))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_EQ("token_2", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_3_.get());
      }));
  EXPECT_CALL(*clusters_reader_1_, Finish(_, _, _))
      .WillOnce(Invoke(create_list_clusters_lambda("token_1", {"cluster_1"},
                                                   {"failed_loc_1"})));
  EXPECT_CALL(*clusters_reader_2_, Finish(_, _, _))
      .WillOnce(Invoke(
          create_list_clusters_lambda("token_2", {"cluster_2", "cluster_3"},
                                      {"failed_loc_1", "failed_loc_2"})));
  EXPECT_CALL(*clusters_reader_3_, Finish(_, _, _))
      .WillOnce(Invoke(
          create_list_clusters_lambda("", {"cluster_4"}, {"failed_loc_1"})));

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
  std::vector<std::string> const expected_clusters{
      "cluster_1",
      "cluster_2",
      "cluster_3",
      "cluster_4",
  };
  EXPECT_EQ(expected_clusters, ClusterNames(user_res_));
  std::vector<std::string> expected_failed_locations{"failed_loc_1",
                                                     "failed_loc_2"};
  EXPECT_EQ(expected_failed_locations, user_res_.failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 2 pages, with a filure between them.
TEST_F(NoexAsyncListClustersTest, FailuresAreRetried) {
  EXPECT_CALL(*client_, AsyncListClusters(_, _, _))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_1_.get());
      }))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_2_.get());
      }))
      .WillOnce(Invoke([this](grpc::ClientContext*,
                              btproto::ListClustersRequest const& request,
                              grpc::CompletionQueue*) {
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListClustersResponse>>(
            clusters_reader_3_.get());
      }));
  EXPECT_CALL(*clusters_reader_1_, Finish(_, _, _))
      .WillOnce(Invoke(create_list_clusters_lambda("token_1", {"cluster_1"},
                                                   {"failed_loc_1"})));
  EXPECT_CALL(*clusters_reader_2_, Finish(_, _, _))
      .WillOnce(Invoke([](btproto::ListClustersResponse* response,
                          grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "");
      }));
  EXPECT_CALL(*clusters_reader_3_, Finish(_, _, _))
      .WillOnce(Invoke(
          create_list_clusters_lambda("", {"cluster_2"}, {"failed_loc_2"})));

  op_->Start(cq_);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // the timer

  EXPECT_FALSE(user_op_called_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_TRUE(user_op_called_);
  EXPECT_TRUE(user_status_.ok());
  std::vector<std::string> const expected_clusters{
      "cluster_1",
      "cluster_2",
  };
  EXPECT_EQ(expected_clusters, ClusterNames(user_res_));
  std::vector<std::string> expected_failed_locations{"failed_loc_1",
                                                     "failed_loc_2"};
  EXPECT_EQ(expected_failed_locations, user_res_.failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
