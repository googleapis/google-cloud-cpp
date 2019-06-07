// Copyright 2019 Google LLC
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

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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

std::string const kProjectId = "the-project";

class AsyncListClustersTest : public ::testing::Test {
 public:
  AsyncListClustersTest()
      : cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new testing::MockInstanceAdminClient),
        metadata_update_policy_("my_instance", MetadataParamTypes::NAME),
        clusters_reader_1_(new MockAsyncListClustersReader),
        clusters_reader_2_(new MockAsyncListClustersReader),
        clusters_reader_3_(new MockAsyncListClustersReader) {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

 protected:
  void Start() {
    InstanceAdmin instance_admin(client_);
    user_future_ = instance_admin.AsyncListClusters(cq_, "my_instance");
  }

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<testing::MockInstanceAdminClient> client_;
  future<StatusOr<ClusterList>> user_future_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<MockAsyncListClustersReader> clusters_reader_1_;
  std::unique_ptr<MockAsyncListClustersReader> clusters_reader_2_;
  std::unique_ptr<MockAsyncListClustersReader> clusters_reader_3_;
};

// Dynamically create the lambda for `Invoke()`, note that the return type is
// unknown, so a function or function template would not work. Alternatively,
// writing this inline is very repetitive.
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
  std::sort(res.begin(), res.end());
  return res;
}

/// @test One successful page with 1 one cluster.
TEST_F(AsyncListClustersTest, Simple) {
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

  Start();

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  EXPECT_EQ(std::vector<std::string>{"cluster_1"}, ClusterNames(*res));
  EXPECT_EQ(std::vector<std::string>{"failed_loc_1"}, res->failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 3 pages, no failures, multiple clusters and failed locations.
TEST_F(AsyncListClustersTest, MultipleClustersAndLocations) {
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

  Start();

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  std::vector<std::string> const expected_clusters{
      "cluster_1",
      "cluster_2",
      "cluster_3",
      "cluster_4",
  };
  EXPECT_EQ(expected_clusters, ClusterNames(*res));
  std::vector<std::string> expected_failed_locations{"failed_loc_1",
                                                     "failed_loc_2"};
  std::sort(res->failed_locations.begin(), res->failed_locations.end());
  EXPECT_EQ(expected_failed_locations, res->failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 2 pages, with a filure between them.
TEST_F(AsyncListClustersTest, FailuresAreRetried) {
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
      .WillOnce(Invoke(
          [](btproto::ListClustersResponse*, grpc::Status* status, void*) {
            *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "");
          }));
  EXPECT_CALL(*clusters_reader_3_, Finish(_, _, _))
      .WillOnce(Invoke(
          create_list_clusters_lambda("", {"cluster_2"}, {"failed_loc_2"})));

  Start();

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // the timer

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  std::vector<std::string> const expected_clusters{
      "cluster_1",
      "cluster_2",
  };
  EXPECT_EQ(expected_clusters, ClusterNames(*res));
  std::vector<std::string> expected_failed_locations{"failed_loc_1",
                                                     "failed_loc_2"};
  std::sort(res->failed_locations.begin(), res->failed_locations.end());
  EXPECT_EQ(expected_failed_locations, res->failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
