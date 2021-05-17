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
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
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

namespace btadmin = google::bigtable::admin::v2;

using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::testing::ReturnRef;

using MockAsyncListAppProfilesReader =
    ::google::cloud::testing_util::MockAsyncResponseReader<
        btadmin::ListAppProfilesResponse>;
using Functor = std::function<void(
    CompletionQueue&, std::vector<btadmin::AppProfile>&, grpc::Status&)>;

std::string const kProjectId = "the-project";

class AsyncListAppProfilesTest : public ::testing::Test {
 public:
  AsyncListAppProfilesTest()
      : cq_impl_(new FakeCompletionQueueImpl),
        cq_(cq_impl_),
        client_(new testing::MockInstanceAdminClient),
        profiles_reader_1_(new MockAsyncListAppProfilesReader),
        profiles_reader_2_(new MockAsyncListAppProfilesReader),
        profiles_reader_3_(new MockAsyncListAppProfilesReader) {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

 protected:
  void Start() {
    InstanceAdmin instance_admin(client_);
    user_future_ = instance_admin.AsyncListAppProfiles(cq_, "my_instance");
  }

  std::shared_ptr<FakeCompletionQueueImpl> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<testing::MockInstanceAdminClient> client_;
  future<StatusOr<std::vector<btadmin::AppProfile>>> user_future_;
  std::unique_ptr<MockAsyncListAppProfilesReader> profiles_reader_1_;
  std::unique_ptr<MockAsyncListAppProfilesReader> profiles_reader_2_;
  std::unique_ptr<MockAsyncListAppProfilesReader> profiles_reader_3_;
};

// Dynamically create the lambda for `Finish()`. Note that the return type is
// unknown, so a function or function template would not work. Alternatively,
// writing this inline is very repetitive.
auto create_list_profiles_lambda =
    [](std::string const& returned_token,
       std::vector<std::string> const& profile_names) {
      return [returned_token, profile_names](
                 btadmin::ListAppProfilesResponse* response,
                 grpc::Status* status, void*) {
        for (auto const& app_profile_name : profile_names) {
          auto& app_profile = *response->add_app_profiles();
          app_profile.set_name(app_profile_name);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        *status = grpc::Status::OK;
      };
    };

std::vector<std::string> AppProfileNames(
    std::vector<btadmin::AppProfile> const& response) {
  std::vector<std::string> res;
  std::transform(response.begin(), response.end(), std::back_inserter(res),
                 [](btadmin::AppProfile const& app_profile) {
                   return app_profile.name();
                 });
  return res;
}

/// @test One successful page with 1 one profile.
TEST_F(AsyncListAppProfilesTest, Simple) {
  EXPECT_CALL(*client_, AsyncListAppProfiles)
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_1_.get());
      });
  EXPECT_CALL(*profiles_reader_1_, Finish)
      .WillOnce(create_list_profiles_lambda("", {"profile_1"}));

  Start();

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  EXPECT_EQ(std::vector<std::string>{"profile_1"}, AppProfileNames(*res));
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 3 pages, no failures, multiple profiles.
TEST_F(AsyncListAppProfilesTest, MultipleProfiles) {
  EXPECT_CALL(*client_, AsyncListAppProfiles)
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_1_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_2_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_2", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_3_.get());
      });
  EXPECT_CALL(*profiles_reader_1_, Finish)
      .WillOnce(create_list_profiles_lambda("token_1", {"profile_1"}));
  EXPECT_CALL(*profiles_reader_2_, Finish)
      .WillOnce(
          create_list_profiles_lambda("token_2", {"profile_2", "profile_3"}));
  EXPECT_CALL(*profiles_reader_3_, Finish)
      .WillOnce(create_list_profiles_lambda("", {"profile_4"}));

  Start();

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  std::vector<std::string> const expected_profiles{
      "profile_1",
      "profile_2",
      "profile_3",
      "profile_4",
  };
  EXPECT_EQ(expected_profiles, AppProfileNames(*res));
  EXPECT_TRUE(cq_impl_->empty());
}

/// @test Test 2 pages, with a failure between them.
TEST_F(AsyncListAppProfilesTest, FailuresAreRetried) {
  EXPECT_CALL(*client_, AsyncListAppProfiles)
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_TRUE(request.page_token().empty());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_1_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_2_.get());
      })
      .WillOnce([this](grpc::ClientContext* context,
                       btadmin::ListAppProfilesRequest const& request,
                       grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListAppProfiles",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ("token_1", request.page_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::ListAppProfilesResponse>>(profiles_reader_3_.get());
      });
  EXPECT_CALL(*profiles_reader_1_, Finish)
      .WillOnce(create_list_profiles_lambda("token_1", {"profile_1"}));
  EXPECT_CALL(*profiles_reader_2_, Finish)
      .WillOnce(
          [](btadmin::ListAppProfilesResponse*, grpc::Status* status, void*) {
            *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "");
          });
  EXPECT_CALL(*profiles_reader_3_, Finish)
      .WillOnce(create_list_profiles_lambda("", {"profile_2"}));

  Start();

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // the timer

  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto res = user_future_.get();
  EXPECT_STATUS_OK(res);
  std::vector<std::string> const expected_profiles{
      "profile_1",
      "profile_2",
  };
  EXPECT_EQ(expected_profiles, AppProfileNames(*res));
  EXPECT_TRUE(cq_impl_->empty());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
