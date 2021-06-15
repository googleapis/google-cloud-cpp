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
#include "google/cloud/credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

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

bool RunQuotaLimitedTests() {
  static bool run_quota_limited_tests =
      internal::GetEnv("GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_INTEGRATION_TESTS")
          .value_or("") == "yes";
  return run_quota_limited_tests;
}

class IamIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
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
  std::string iam_project_;
  std::string iam_service_account_;
  std::string invalid_iam_service_account_;

 private:
  testing_util::ScopedLog log_;
};

Options TestFailureOptions() {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::minutes(15);
  return Options{}
      .set<TracingComponentsOption>({"rpc"})
      .set<UnifiedCredentialsOption>(
          MakeAccessTokenCredentials("invalid-access-token", expiration))
      .set<IAMRetryPolicyOption>(IAMLimitedErrorCountRetryPolicy(1).clone())
      .set<IAMBackoffPolicyOption>(
          ExponentialBackoffPolicy(std::chrono::seconds(1),
                                   std::chrono::seconds(1), 2.0)
              .clone());
}

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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.ListServiceAccounts("projects/invalid");
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, Not(IsOk()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.GetServiceAccount("projects/-/serviceAccounts/" +
                                           invalid_iam_service_account_);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccount")));
}

TEST_F(IamIntegrationTest, CreateServiceAccountFailure) {
  ::google::iam::admin::v1::ServiceAccount service_account;
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.CreateServiceAccount("", "", service_account);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateServiceAccount")));
}

TEST_F(IamIntegrationTest, ServiceAccountCrudSuccess) {
  if (!RunQuotaLimitedTests()) GTEST_SKIP();
  auto client = IAMClient(MakeIAMConnection());
  std::string account_id = "sa-crud-test";
  auto service_account_inferred_name =
      absl::StrCat("projects/-/serviceAccounts/", account_id, "@", iam_project_,
                   ".iam.gserviceaccount.com");

  // In case a previous execution left the service account.
  client.DeleteServiceAccount(service_account_inferred_name);

  ::google::iam::admin::v1::ServiceAccount service_account;
  service_account.set_display_name(account_id);
  service_account.set_description(
      "Service account created during IAM integration test.");
  auto create_response = client.CreateServiceAccount(
      "projects/" + iam_project_, account_id, service_account);
  ASSERT_STATUS_OK(create_response);

  auto delete_response =
      client.DeleteServiceAccount(service_account_inferred_name);
  // Service Account may not be usable for up to 60s after creation.
  if (delete_response.code() == StatusCode::kNotFound) {
    std::this_thread::sleep_for(std::chrono::seconds(61));
    delete_response =
        client.DeleteServiceAccount(service_account_inferred_name);
  }
  ASSERT_STATUS_OK(delete_response);
}

TEST_F(IamIntegrationTest, DeleteServiceAccountFailure) {
  ::google::iam::admin::v1::ServiceAccount service_account;
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.DeleteServiceAccount("");
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteServiceAccount")));
}

TEST_F(IamIntegrationTest, ListServiceAccountKeysFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.ListServiceAccountKeys(
      "projects/-/serviceAccounts/" + invalid_iam_service_account_, {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST_F(IamIntegrationTest, GetServiceAccountKeyFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.GetServiceAccountKey(
      "projects/-/serviceAccounts/" + invalid_iam_service_account_, {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccountKey")));
}

TEST_F(IamIntegrationTest, CreateServiceAccountKeyFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.DeleteServiceAccountKey("projects/-/serviceAccounts/" +
                                                 invalid_iam_service_account_);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteServiceAccountKey")));
}

TEST_F(IamIntegrationTest, ServiceAccountKeyCrudSuccess) {
  if (!RunQuotaLimitedTests()) GTEST_SKIP();
  auto client = IAMClient(MakeIAMConnection());
  auto create_response = client.CreateServiceAccountKey(
      "projects/-/serviceAccounts/" + iam_service_account_,
      ::google::iam::admin::v1::ServiceAccountPrivateKeyType::
          TYPE_GOOGLE_CREDENTIALS_FILE,
      ::google::iam::admin::v1::ServiceAccountKeyAlgorithm::KEY_ALG_RSA_2048);
  ASSERT_STATUS_OK(create_response);
  EXPECT_GT(create_response->private_key_data().size(), 0);

  auto get_response = client.GetServiceAccountKey(
      create_response->name(),
      ::google::iam::admin::v1::ServiceAccountPublicKeyType::
          TYPE_X509_PEM_FILE);
  // Key may not be usable for up to 60 seconds after creation.
  if (get_response.status().code() == StatusCode::kNotFound) {
    std::this_thread::sleep_for(std::chrono::seconds(61));
    get_response = client.GetServiceAccountKey(
        create_response->name(),
        ::google::iam::admin::v1::ServiceAccountPublicKeyType::
            TYPE_X509_PEM_FILE);
  }
  ASSERT_STATUS_OK(get_response);
  EXPECT_GT(get_response->public_key_data().size(), 0);

  auto list_response = client.ListServiceAccountKeys(
      "projects/-/serviceAccounts/" + iam_service_account_,
      {::google::iam::admin::v1::ListServiceAccountKeysRequest::USER_MANAGED});
  ASSERT_STATUS_OK(list_response);
  std::vector<std::string> key_names;
  for (auto const& key : list_response->keys()) {
    key_names.push_back(key.name());
  }
  EXPECT_THAT(key_names, Contains(create_response->name()));

  for (auto const& key : list_response->keys()) {
    auto delete_response = client.DeleteServiceAccountKey(key.name());
    EXPECT_STATUS_OK(delete_response);
  }
}

TEST_F(IamIntegrationTest, GetIamPolicySuccess) {
  auto client = IAMClient(MakeIAMConnection());
  auto response = client.GetIamPolicy(absl::StrCat(
      "projects/", iam_project_, "/serviceAccounts/", iam_service_account_));
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, GetIamPolicyFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.GetIamPolicy("");
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
}

TEST_F(IamIntegrationTest, SetIamPolicySuccess) {
  if (!RunQuotaLimitedTests()) GTEST_SKIP();
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::v1::Policy policy;
  auto response = client.SetIamPolicy(
      absl::StrCat("projects/", iam_project_, "/serviceAccounts/",
                   iam_service_account_),
      policy);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, SetIamPolicyFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  auto response = client.QueryGrantableRoles("");
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, Not(IsOk()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::ListServiceAccountsRequest request;
  auto response = client.ListServiceAccounts(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, Not(IsOk()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::GetServiceAccountRequest request;
  auto response = client.GetServiceAccount(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccount")));
}

TEST_F(IamIntegrationTest, ServiceAccountCrudProtoSuccess) {
  if (!RunQuotaLimitedTests()) GTEST_SKIP();
  auto client = IAMClient(MakeIAMConnection());
  std::string account_id = "sa-crud-proto-test";
  auto service_account_inferred_name =
      absl::StrCat("projects/-/serviceAccounts/", account_id, "@", iam_project_,
                   ".iam.gserviceaccount.com");

  // In case a previous execution left the service account.
  ::google::iam::admin::v1::DeleteServiceAccountRequest pre_delete_request;
  pre_delete_request.set_name(service_account_inferred_name);
  client.DeleteServiceAccount(pre_delete_request);

  ::google::iam::admin::v1::CreateServiceAccountRequest create_request;
  create_request.set_name("projects/" + iam_project_);
  create_request.set_account_id(account_id);
  ::google::iam::admin::v1::ServiceAccount service_account;
  service_account.set_display_name(account_id);
  service_account.set_description(
      "Service account created during IAM integration test.");
  *create_request.mutable_service_account() = service_account;
  auto create_response = client.CreateServiceAccount(create_request);
  ASSERT_STATUS_OK(create_response);
  auto unique_id = create_response->unique_id();
  auto service_account_full_name =
      absl::StrCat("projects/", iam_project_, "/serviceAccounts/", unique_id);

  ::google::iam::admin::v1::DisableServiceAccountRequest disable_request;
  disable_request.set_name(service_account_inferred_name);
  auto disable_response = client.DisableServiceAccount(disable_request);
  // Service Account may not be usable for up to 60s after creation.
  if (disable_response.code() == StatusCode::kNotFound) {
    std::this_thread::sleep_for(std::chrono::seconds(61));
    disable_response = client.DisableServiceAccount(disable_request);
  }
  EXPECT_STATUS_OK(disable_response);

  ::google::iam::admin::v1::EnableServiceAccountRequest enable_request;
  enable_request.set_name(service_account_inferred_name);
  auto enable_response = client.DisableServiceAccount(disable_request);
  EXPECT_STATUS_OK(enable_response);

  ::google::iam::admin::v1::PatchServiceAccountRequest patch_request;
  ::google::iam::admin::v1::ServiceAccount patch_service_account;
  patch_service_account.set_name(service_account_inferred_name);
  patch_service_account.set_description("Patched");
  *patch_request.mutable_service_account() = patch_service_account;
  google::protobuf::FieldMask update_mask;
  *update_mask.add_paths() = "description";
  *patch_request.mutable_update_mask() = update_mask;
  auto patch_response = client.PatchServiceAccount(patch_request);
  // TODO(#6475): Determine how to make this call successful.
  EXPECT_THAT(patch_response, StatusIs(StatusCode::kFailedPrecondition));

  ::google::iam::admin::v1::DeleteServiceAccountRequest delete_request;
  delete_request.set_name(service_account_inferred_name);
  auto delete_response = client.DeleteServiceAccount(delete_request);
  ASSERT_STATUS_OK(delete_response);

  ::google::iam::admin::v1::UndeleteServiceAccountRequest undelete_request;
  undelete_request.set_name(service_account_full_name);
  auto undelete_response = client.UndeleteServiceAccount(undelete_request);
  ASSERT_STATUS_OK(undelete_response);
  EXPECT_EQ(undelete_response->restored_account().unique_id(), unique_id);

  auto really_delete_response = client.DeleteServiceAccount(delete_request);
  EXPECT_STATUS_OK(really_delete_response);
}

TEST_F(IamIntegrationTest, EnableServiceAccountProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::EnableServiceAccountRequest request;
  auto response = client.EnableServiceAccount(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("EnableServiceAccount")));
}

TEST_F(IamIntegrationTest, DisableServiceAccountProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::DisableServiceAccountRequest request;
  auto response = client.DisableServiceAccount(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DisableServiceAccount")));
}

TEST_F(IamIntegrationTest, ListServiceAccountKeysProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::ListServiceAccountKeysRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.ListServiceAccountKeys(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST_F(IamIntegrationTest, GetServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::GetServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.GetServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetServiceAccountKey")));
}

TEST_F(IamIntegrationTest, CreateServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::CreateServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.CreateServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateServiceAccountKey")));
}

TEST_F(IamIntegrationTest, UploadServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::UploadServiceAccountKeyRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   invalid_iam_service_account_);
  auto response = client.UploadServiceAccountKey(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UploadServiceAccountKey")));
}

TEST_F(IamIntegrationTest, DeleteServiceAccountKeyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::v1::GetIamPolicyRequest request;
  auto response = client.GetIamPolicy(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
}

TEST_F(IamIntegrationTest, SetIamPolicyProtoSuccess) {
  if (!RunQuotaLimitedTests()) GTEST_SKIP();
  auto client = IAMClient(MakeIAMConnection());
  ::google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(absl::StrCat("projects/", iam_project_,
                                    "/serviceAccounts/", iam_service_account_));
  ::google::iam::v1::Policy policy;
  auto response = client.SetIamPolicy(request);
  EXPECT_STATUS_OK(response);
}

TEST_F(IamIntegrationTest, SetIamPolicyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::QueryGrantableRolesRequest request;
  auto response = client.QueryGrantableRoles(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("QueryGrantableRoles")));
}

TEST_F(IamIntegrationTest, ListRolesProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::ListRolesRequest request;
  request.set_parent("projects/*");
  auto response = client.ListRoles(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListRoles")));
}

TEST_F(IamIntegrationTest, GetRoleProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::GetRoleRequest request;
  request.set_name("projects/*");
  auto response = client.GetRole(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetRole")));
}

TEST_F(IamIntegrationTest, CreateRoleProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::CreateRoleRequest request;
  auto response = client.CreateRole(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateRole")));
}

TEST_F(IamIntegrationTest, UpdateRoleProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::UpdateRoleRequest request;
  auto response = client.UpdateRole(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateRole")));
}

TEST_F(IamIntegrationTest, DeleteRoleProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::DeleteRoleRequest request;
  auto response = client.DeleteRole(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteRole")));
}

TEST_F(IamIntegrationTest, UndeleteRoleProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::UndeleteRoleRequest request;
  auto response = client.UndeleteRole(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UndeleteRole")));
}

TEST_F(IamIntegrationTest, RoleProtoCrudSuccess) {
  if (!RunQuotaLimitedTests()) GTEST_SKIP();
  auto client = IAMClient(MakeIAMConnection());
  auto const parent_project = "projects/" + iam_project_;
  // Clean up any roles leaked in previous executions.
  ::google::iam::admin::v1::ListRolesRequest list_request;
  list_request.set_parent(parent_project);
  auto list_response = client.ListRoles(list_request);
  for (auto const& role : list_response) {
    ASSERT_STATUS_OK(role);
    if (absl::StartsWith(role->name(), "iam_test_role")) {
      ::google::iam::admin::v1::DeleteRoleRequest delete_request;
      delete_request.set_name(role->name());
      auto delete_response = client.DeleteRole(delete_request);
      EXPECT_STATUS_OK(delete_response);
    }
  }

  auto role_id = absl::StrCat(
      "iam_test_role_",
      absl::FormatTime("%Y%m%d%H%M%S", absl::Now(), absl::UTCTimeZone()));
  ::google::iam::admin::v1::CreateRoleRequest create_request;
  create_request.set_parent(parent_project);
  create_request.set_role_id(role_id);
  ::google::iam::admin::v1::Role role;
  *role.add_included_permissions() = "iam.serviceAccounts.list";
  role.set_stage(::google::iam::admin::v1::Role::DISABLED);
  *create_request.mutable_role() = role;
  auto create_response = client.CreateRole(create_request);
  ASSERT_STATUS_OK(create_response);

  ::google::iam::admin::v1::GetRoleRequest get_request;
  get_request.set_name(create_response->name());
  auto get_response = client.GetRole(get_request);
  EXPECT_STATUS_OK(get_response);

  list_response = client.ListRoles(list_request);
  std::vector<std::string> role_ids;
  for (auto const& r : list_response) {
    if (r) {
      role_ids.push_back(r->name());
    }
  }
  auto expected_role_id =
      absl::StrCat("projects/", iam_project_, "/roles/", role_id);
  EXPECT_THAT(role_ids, Contains(expected_role_id));

  ::google::iam::admin::v1::UpdateRoleRequest update_request;
  update_request.set_name(create_response->name());
  ::google::iam::admin::v1::Role updated_role;
  updated_role.set_title("Test Role Please Ignore");
  ::google::protobuf::FieldMask update_mask;
  *update_mask.add_paths() = "title";
  *update_request.mutable_role() = updated_role;
  *update_request.mutable_update_mask() = update_mask;
  auto update_response = client.UpdateRole(update_request);
  EXPECT_STATUS_OK(update_response);

  ::google::iam::admin::v1::DeleteRoleRequest delete_request;
  delete_request.set_name(create_response->name());
  auto delete_response = client.DeleteRole(delete_request);
  ASSERT_STATUS_OK(delete_response);

  ::google::iam::admin::v1::UndeleteRoleRequest undelete_request;
  undelete_request.set_name(create_response->name());
  auto undelete_response = client.UndeleteRole(undelete_request);
  EXPECT_STATUS_OK(undelete_response);

  auto final_delete_response = client.DeleteRole(delete_request);
  EXPECT_STATUS_OK(final_delete_response);
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
  ::google::iam::admin::v1::QueryTestablePermissionsRequest request;
  auto response = client.QueryTestablePermissions(request);
  auto begin = response.begin();
  ASSERT_NE(begin, response.end());
  EXPECT_THAT(*begin, Not(IsOk()));
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
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
  ASSERT_STATUS_OK(response);
  EXPECT_GT(response->lint_results().size(), 0);
}

TEST_F(IamIntegrationTest, LintPolicyProtoFailure) {
  auto client = IAMClient(MakeIAMConnection(TestFailureOptions()));
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
