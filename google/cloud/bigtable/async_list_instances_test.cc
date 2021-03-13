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
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <thread>
// TODO(#5923) - remove after deprecation is completed
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = google::bigtable::admin::v2;
using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::testing::_;
using ::testing::ReturnRef;
using MockAsyncListInstancesReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        btproto::ListInstancesResponse>;
using google::cloud::testing_util::FakeCompletionQueueImpl;

using Functor =
    std::function<void(CompletionQueue&, InstanceList&, grpc::Status&)>;

std::string const kProjectId = "the-project";

class AsyncListInstancesTest : public ::testing::Test {
 public:
  AsyncListInstancesTest()
      : cq_impl_(new FakeCompletionQueueImpl),
        cq_(cq_impl_),
        client_(new testing::MockInstanceAdminClient),
        instances_reader_1_(new MockAsyncListInstancesReader),
        instances_reader_2_(new MockAsyncListInstancesReader),
        instances_reader_3_(new MockAsyncListInstancesReader) {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

 protected:
  void Start() {
    InstanceAdmin instance_admin(client_);
    user_future_ = instance_admin.AsyncListInstances(cq_);
  }

  std::shared_ptr<FakeCompletionQueueImpl> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<testing::MockInstanceAdminClient> client_;
  future<StatusOr<InstanceList>> user_future_;
  std::unique_ptr<MockAsyncListInstancesReader> instances_reader_1_;
  std::unique_ptr<MockAsyncListInstancesReader> instances_reader_2_;
  std::unique_ptr<MockAsyncListInstancesReader> instances_reader_3_;
};

// Dynamically create the lambda for `Finish()`. Note that the return type is
// unknown, so a function or function template would not work. Alternatively,
// writing this inline is very repetitive.
auto create_list_instances_lambda =
    [](std::string const& returned_token,
       std::vector<std::string> const& instance_names,
       std::vector<std::string> const& failed_locations) {
      return [returned_token, instance_names, failed_locations](
                 btproto::ListInstancesResponse* response, grpc::Status* status,
                 void*) {
        for (auto const& instance_name : instance_names) {
          auto& instance = *response->add_instances();
          instance.set_name(instance_name);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        for (auto const& failed_location : failed_locations) {
          response->add_failed_locations(failed_location);
        }
        *status = grpc::Status::OK;
      };
    };

std::vector<std::string> InstanceNames(InstanceList const& response) {
  std::vector<std::string> res;
  std::transform(
      response.instances.begin(), response.instances.end(),
      std::back_inserter(res),
      [](btproto::Instance const& instance) { return instance.name(); });
  return res;
}

/// @test One successful page with 1 one instance.
TEST_F(AsyncListInstancesTest, Simple) {
  EXPECT_CALL(*client_, AsyncListInstances(_, _, _))
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_1_.get());
      });
  EXPECT_CALL(*instances_reader_1_, Finish(_, _, _))
      .WillOnce(
          create_list_instances_lambda("", {"instance_1"}, {"failed_loc_1"}));

  Start();

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  EXPECT_EQ(std::vector<std::string>{"instance_1"}, InstanceNames(*res));
  EXPECT_EQ(std::vector<std::string>{"failed_loc_1"}, res->failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 3 pages, no failures, multiple clusters and failed locations.
TEST_F(AsyncListInstancesTest, MultipleInstancesAndLocations) {
  EXPECT_CALL(*client_, AsyncListInstances(_, _, _))
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_1_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_2_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_2", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_3_.get());
      });
  EXPECT_CALL(*instances_reader_1_, Finish(_, _, _))
      .WillOnce(create_list_instances_lambda("token_1", {"instance_1"},
                                             {"failed_loc_1"}));
  EXPECT_CALL(*instances_reader_2_, Finish(_, _, _))
      .WillOnce(create_list_instances_lambda("token_2",
                                             {"instance_2", "instance_3"},
                                             {"failed_loc_1", "failed_loc_2"}));
  EXPECT_CALL(*instances_reader_3_, Finish(_, _, _))
      .WillOnce(
          create_list_instances_lambda("", {"instance_4"}, {"failed_loc_1"}));

  Start();

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  std::vector<std::string> const expected_instances{
      "instance_1",
      "instance_2",
      "instance_3",
      "instance_4",
  };
  EXPECT_EQ(expected_instances, InstanceNames(*res));
  std::vector<std::string> expected_failed_locations{"failed_loc_1",
                                                     "failed_loc_2"};
  std::sort(res->failed_locations.begin(), res->failed_locations.end());
  EXPECT_EQ(expected_failed_locations, res->failed_locations);
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 2 pages, with a failure between them.
TEST_F(AsyncListInstancesTest, FailuresAreRetried) {
  EXPECT_CALL(*client_, AsyncListInstances(_, _, _))
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_1_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_2_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btproto::ListInstancesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::bigtable::admin::v2::ListInstancesResponse>>(
            instances_reader_3_.get());
      });
  EXPECT_CALL(*instances_reader_1_, Finish(_, _, _))
      .WillOnce(create_list_instances_lambda("token_1", {"instance_1"},
                                             {"failed_loc_1"}));
  EXPECT_CALL(*instances_reader_2_, Finish(_, _, _))
      .WillOnce(
          [](btproto::ListInstancesResponse*, grpc::Status* status, void*) {
            *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "");
          });
  EXPECT_CALL(*instances_reader_3_, Finish(_, _, _))
      .WillOnce(
          create_list_instances_lambda("", {"instance_2"}, {"failed_loc_2"}));

  Start();

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer

  EXPECT_EQ(user_future_.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  std::vector<std::string> const expected_instances{
      "instance_1",
      "instance_2",
  };
  EXPECT_EQ(expected_instances, InstanceNames(*res));
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
