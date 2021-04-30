// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/iam/iam_client.h"
#include "google/cloud/iam/iam_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace iam {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;

class IamIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    options_.set<TracingComponentsOption>({"rpc"});
    retry_policy_ = absl::make_unique<IAMLimitedErrorCountRetryPolicy>(1);
    backoff_policy_ = absl::make_unique<ExponentialBackoffPolicy>(
        std::chrono::seconds(1), std::chrono::seconds(1), 2.0);
    iam_project_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    iam_service_account_ = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT")
                               .value_or("");
    invalid_iam_service_account_ =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT")
            .value_or("");

    ASSERT_FALSE(iam_project_.empty());
    ASSERT_FALSE(iam_service_account_.empty());
    ASSERT_FALSE(invalid_iam_service_account_.empty());
  }
  std::vector<std::string> ClearLogLines() { return log_.ExtractLines(); }
  Options options_;
  std::unique_ptr<IAMRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  std::string iam_project_;
  std::string iam_service_account_;
  std::string invalid_iam_service_account_;

 private:
  testing_util::ScopedLog log_;
};

TEST_F(IamIntegrationTest, ListServiceAccountsSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto expected_service_account = absl::StrCat(
      "projects/", iam_project_, "/serviceAccounts/", iam_service_account_);
  auto response = client.ListServiceAccounts("projects/" + iam_project_);
  std::vector<std::string> service_account_names;
  for (auto const& account : response) {
    ASSERT_STATUS_OK(account);
    service_account_names.push_back(account->name());
  }
  EXPECT_THAT(service_account_names, Contains(expected_service_account));
}

TEST_F(IamIntegrationTest, ListServiceAccountsFailure) {
  options_.set<IAMRetryPolicyOption>(retry_policy_->clone());
  options_.set<IAMBackoffPolicyOption>(backoff_policy_->clone());
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.ListServiceAccounts("projects/invalid");
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kNotFound));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccounts")));
}

TEST_F(IamIntegrationTest, GetServiceAccountSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.GetServiceAccount("projects/-/serviceAccounts/" +
                                           iam_service_account_);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->unique_id().empty());
}

TEST_F(IamIntegrationTest, GetServiceAccountFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.GetServiceAccount("projects/-/serviceAccounts/" +
                                           invalid_iam_service_account_);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccount")));
}

#if 0
TEST_F(IamIntegrationTest, CreateServiceAccountSuccess) {
::google::iam::admin::v1::ServiceAccount service_account;
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.CreateServiceAccount(
      iam_project_, "iam-integration-test", service_account);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE();
}
#endif

TEST_F(IamIntegrationTest, CreateServiceAccountFailure) {
  ::google::iam::admin::v1::ServiceAccount service_account;
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.CreateServiceAccount("", "", service_account);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateServiceAccount")));
}

#if 0
TEST_F(IamIntegrationTest, DeleteServiceAccountSuccess) {
::google::iam::admin::v1::ServiceAccount service_account;
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.DeleteServiceAccount("");
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE();
}
#endif

TEST_F(IamIntegrationTest, DeleteServiceAccountFailure) {
  ::google::iam::admin::v1::ServiceAccount service_account;
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.DeleteServiceAccount("");
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteServiceAccount")));
}

TEST_F(IamIntegrationTest, ListServiceAccountKeysFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.ListServiceAccountKeys(
      "projects/-/serviceAccounts/" + invalid_iam_service_account_, {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST_F(IamIntegrationTest, GetServiceAccountKeyFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.GetServiceAccountKey(
      "projects/-/serviceAccounts/" + invalid_iam_service_account_, {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccountKey")));
}

TEST_F(IamIntegrationTest, CreateServiceAccountKeyFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.CreateServiceAccountKey(
      "projects/-/serviceAccounts/" + invalid_iam_service_account_,
      ::google::iam::admin::v1::ServiceAccountPrivateKeyType::
          TYPE_GOOGLE_CREDENTIALS_FILE,
      ::google::iam::admin::v1::ServiceAccountKeyAlgorithm::KEY_ALG_RSA_2048);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateServiceAccountKey")));
}

TEST_F(IamIntegrationTest, DeleteServiceAccountKeyFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.DeleteServiceAccountKey("projects/-/serviceAccounts/" +
                                                 invalid_iam_service_account_);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteServiceAccountKey")));
}

TEST_F(IamIntegrationTest, ServiceAccountKeyCrudSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto create_response = client.CreateServiceAccountKey(
      "projects/-/serviceAccounts/" + iam_service_account_,
      ::google::iam::admin::v1::ServiceAccountPrivateKeyType::
          TYPE_GOOGLE_CREDENTIALS_FILE,
      ::google::iam::admin::v1::ServiceAccountKeyAlgorithm::KEY_ALG_RSA_2048);
  ASSERT_STATUS_OK(create_response);
  EXPECT_GT(create_response->private_key_data().size(), 0);

  auto list_response = client.ListServiceAccountKeys(
      "projects/-/serviceAccounts/" + iam_service_account_, {});
  ASSERT_STATUS_OK(list_response);
  EXPECT_GT(list_response->keys().size(), 0);

  auto get_response = client.GetServiceAccountKey(
      create_response->name(),
      ::google::iam::admin::v1::ServiceAccountPublicKeyType::
          TYPE_X509_PEM_FILE);
  ASSERT_STATUS_OK(get_response);
  EXPECT_GT(get_response->public_key_data().size(), 0);

  auto delete_response =
      client.DeleteServiceAccountKey(create_response->name());
  EXPECT_STATUS_OK(delete_response);
}

TEST_F(IamIntegrationTest, GetIamPolicySuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.GetIamPolicy(absl::StrCat(
      "projects/", iam_project_, "/serviceAccounts/", iam_service_account_));
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, GetIamPolicyFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.GetIamPolicy("");
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
}

TEST_F(IamIntegrationTest, SetIamPolicySuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::v1::Policy policy;
  auto response = client.SetIamPolicy(
      absl::StrCat("projects/", iam_project_, "/serviceAccounts/",
                   iam_service_account_),
      policy);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, SetIamPolicyFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.SetIamPolicy("", {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
}

TEST_F(IamIntegrationTest, TestIamPermissionsSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.TestIamPermissions(
      absl::StrCat("projects/", iam_project_, "/serviceAccounts/",
                   iam_service_account_),
      {"iam.serviceAccounts.getIamPolicy"});
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, TestIamPermissionsFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.TestIamPermissions("", {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
}

TEST_F(IamIntegrationTest, QueryGrantableRolesSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.QueryGrantableRoles(
      absl::StrCat("//iam.googleapis.com/projects/", iam_project_,
                   "/serviceAccounts/", iam_service_account_));
  for (auto const& role : response) {
    EXPECT_STATUS_OK(role);
  }
}

TEST_F(IamIntegrationTest, QueryGrantableRolesFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  auto response = client.QueryGrantableRoles("");
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kInvalidArgument));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("QueryGrantableRoles")));
}

TEST_F(IamIntegrationTest, ListServiceAccountsProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto expected_service_account = absl::StrCat(
      "projects/", iam_project_, "/serviceAccounts/", iam_service_account_);
  ::google::iam::admin::v1::ListServiceAccountsRequest request;
  request.set_name("projects/" + iam_project_);
  auto response = client.ListServiceAccounts(request);
  std::vector<std::string> service_account_names;
  for (auto const& account : response) {
    ASSERT_STATUS_OK(account);
    service_account_names.push_back(account->name());
  }
  EXPECT_THAT(service_account_names, Contains(expected_service_account));
}

TEST_F(IamIntegrationTest, ListServiceAccountsProtoFailure) {
  options_.set<IAMRetryPolicyOption>(retry_policy_->clone());
  options_.set<IAMBackoffPolicyOption>(backoff_policy_->clone());
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::ListServiceAccountsRequest request;
  auto response = client.ListServiceAccounts(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kInvalidArgument));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccounts")));
}

TEST_F(IamIntegrationTest, GetServiceAccountProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::admin::v1::GetServiceAccountRequest request;
  request.set_name("projects/-/serviceAccounts/" + iam_service_account_);
  auto response = client.GetServiceAccount(request);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->unique_id().empty());
}

TEST_F(IamIntegrationTest, GetServiceAccountProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::GetServiceAccountRequest request;
  auto response = client.GetServiceAccount(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccount")));
}

// TODO(sdhart): add this test case into a ServiceAccount crud test case.
// TEST_F(IamIntegrationTest, EnableDisableServiceAccountProtoSuccess) {
//  auto client = IAMClient(MakeIAMConnection(options_));
//  ::google::iam::admin::v1::DisableServiceAccountRequest disable_request;
//  disable_request.set_name("projects/-/serviceAccounts/" +
//  iam_service_account_); auto disable_response =
//  client.DisableServiceAccount(disable_request);
//  EXPECT_STATUS_OK(disable_response);
//  ::google::iam::admin::v1::EnableServiceAccountRequest enable_request;
//  enable_request.set_name("projects/-/serviceAccounts/" +
//  iam_service_account_); auto enable_response =
//  client.DisableServiceAccount(disable_request);
//  EXPECT_STATUS_OK(enable_response);
//}

TEST_F(IamIntegrationTest, EnableServiceAccountProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::EnableServiceAccountRequest request;
  auto response = client.EnableServiceAccount(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("EnableServiceAccount")));
}

TEST_F(IamIntegrationTest, DisableServiceAccountProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::DisableServiceAccountRequest request;
  auto response = client.DisableServiceAccount(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DisableServiceAccount")));
}

TEST_F(IamIntegrationTest, ListServiceAccountKeysProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::ListServiceAccountKeysRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.ListServiceAccountKeys(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST_F(IamIntegrationTest, GetServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::GetServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.GetServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccountKey")));
}

TEST_F(IamIntegrationTest, CreateServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::CreateServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.CreateServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateServiceAccountKey")));
}

TEST_F(IamIntegrationTest, UploadServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::UploadServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.UploadServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UploadServiceAccountKey")));
}

TEST_F(IamIntegrationTest, DeleteServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::DeleteServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.DeleteServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteServiceAccountKey")));
}

TEST_F(IamIntegrationTest, GetIamPolicyProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(absl::StrCat("projects/", iam_project_,
                                    "/serviceAccounts/", iam_service_account_));
  auto response = client.GetIamPolicy(request);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, GetIamPolicyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::v1::GetIamPolicyRequest request;
  auto response = client.GetIamPolicy(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
}

TEST_F(IamIntegrationTest, SetIamPolicyProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(absl::StrCat("projects/", iam_project_,
                                    "/serviceAccounts/", iam_service_account_));
  ::google::iam::v1::Policy policy;
  auto response = client.SetIamPolicy(request);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, SetIamPolicyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::v1::SetIamPolicyRequest request;
  auto response = client.SetIamPolicy(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
}

TEST_F(IamIntegrationTest, TestIamPermissionsProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(absl::StrCat("projects/", iam_project_,
                                    "/serviceAccounts/", iam_service_account_));
  *request.add_permissions() = "iam.serviceAccounts.getIamPolicy";
  auto response = client.TestIamPermissions(request);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, TestIamPermissionsProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto response = client.TestIamPermissions(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
}

TEST_F(IamIntegrationTest, QueryGrantableRolesProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::admin::v1::QueryGrantableRolesRequest request;
  request.set_full_resource_name(absl::StrCat("//iam.googleapis.com/projects/",
                                              iam_project_, "/serviceAccounts/",
                                              iam_service_account_));
  auto response = client.QueryGrantableRoles(request);
  for (auto const& role : response) {
    EXPECT_STATUS_OK(role);
  }
}

TEST_F(IamIntegrationTest, QueryGrantableRolesProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::QueryGrantableRolesRequest request;
  auto response = client.QueryGrantableRoles(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kInvalidArgument));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("QueryGrantableRoles")));
}

TEST_F(IamIntegrationTest, QueryTestablePermissionsProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::admin::v1::QueryTestablePermissionsRequest request;
  request.set_full_resource_name(absl::StrCat("//iam.googleapis.com/projects/",
                                              iam_project_, "/serviceAccounts/",
                                              iam_service_account_));
  auto response = client.QueryTestablePermissions(request);
  for (auto const& role : response) {
    EXPECT_STATUS_OK(role);
  }
}

TEST_F(IamIntegrationTest, QueryTestablePermissionsProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::QueryTestablePermissionsRequest request;
  auto response = client.QueryTestablePermissions(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kInvalidArgument));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("QueryTestablePermissions")));
}

TEST_F(IamIntegrationTest, QueryAuditableServicesProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::admin::v1::QueryAuditableServicesRequest request;
  request.set_full_resource_name(absl::StrCat("//iam.googleapis.com/projects/",
                                              iam_project_, "/serviceAccounts/",
                                              iam_service_account_));
  auto response = client.QueryAuditableServices(request);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, QueryAuditableServicesProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::QueryAuditableServicesRequest request;
  auto response = client.QueryAuditableServices(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("QueryAuditableServices")));
}

TEST_F(IamIntegrationTest, LintPolicyProtoSuccess) {
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::admin::v1::LintPolicyRequest request;
  ::google::type::Expr condition;
  condition.set_expression("request.time < timestamp('2000-01-01T00:00:00Z')");
  request.set_full_resource_name(absl::StrCat("//iam.googleapis.com/projects/",
                                              iam_project_, "/serviceAccounts/",
                                              iam_service_account_));
  *request.mutable_condition() = condition;
  auto response = client.LintPolicy(request);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, LintPolicyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(options_));
  ::google::iam::admin::v1::LintPolicyRequest request;
  auto response = client.LintPolicy(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("LintPolicy")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace iam
}  // namespace cloud
}  // namespace google
