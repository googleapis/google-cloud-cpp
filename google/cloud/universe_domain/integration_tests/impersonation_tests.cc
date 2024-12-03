// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/compute/disks/v1/disks_client.h"
#include "google/cloud/compute/disks/v1/disks_options.h"
#include "google/cloud/kms/v1/key_management_client.h"
#include "google/cloud/kms/v1/key_management_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/location.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace universe_domain {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace gc = ::google::cloud;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;

class DomainUniverseImpersonationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    project_id_ = gc::internal::GetEnv("UD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    zone_id_ = gc::internal::GetEnv("UD_ZONE").value_or("");
    ASSERT_FALSE(zone_id_.empty());
    region_id_ = gc::internal::GetEnv("UD_REGION").value_or("");
    ASSERT_FALSE(region_id_.empty());
  }

  std::string project_id_;
  std::string zone_id_;
  std::string region_id_;
};

class ServiceAccountImpersonationTest : public DomainUniverseImpersonationTest {
 protected:
  void SetUp() override {
    DomainUniverseImpersonationTest::SetUp();

    impersonated_sa_ =
        gc::internal::GetEnv("UD_IMPERSONATED_SERVICE_ACCOUNT_NAME")
            .value_or("");
    ASSERT_FALSE(impersonated_sa_.empty());

    std::string const key_file =
        gc::internal::GetEnv("UD_SA_KEY_FILE").value_or("");
    ASSERT_FALSE(key_file.empty());

    auto is = std::ifstream(key_file);
    is.exceptions(std::ios::badbit);
    credential_ = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  }

  std::string impersonated_sa_;
  std::string credential_;
};

class IdTokenServiceAccountImpersonationTest
    : public DomainUniverseImpersonationTest {
 protected:
  void SetUp() override {
    DomainUniverseImpersonationTest::SetUp();

    key_file_ = gc::internal::GetEnv("UD_IDTOKEN_SA_KEY_FILE").value_or("");
    ASSERT_FALSE(key_file_.empty());
  }

  std::string key_file_;
};

TEST_F(ServiceAccountImpersonationTest, SAToSAImpersonationRest) {
  namespace disks = ::google::cloud::compute_disks_v1;

  gc::Options options;
  options.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeImpersonateServiceAccountCredentials(
          google::cloud::MakeServiceAccountCredentials(credential_),
          impersonated_sa_));

  auto ud_options = gc::AddUniverseDomainOption(gc::ExperimentalTag{}, options);
  ASSERT_STATUS_OK(ud_options);

  auto client = disks::DisksClient(disks::MakeDisksConnectionRest(*ud_options));

  for (auto disk : client.ListDisks(project_id_, zone_id_)) {
    EXPECT_STATUS_OK(disk);
  }
}

TEST_F(ServiceAccountImpersonationTest, SAToSAImpersonationGrpc) {
  namespace kms = ::google::cloud::kms_v1;

  auto const location = gc::Location(project_id_, region_id_);
  gc::Options options;
  options.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeImpersonateServiceAccountCredentials(
          google::cloud::MakeServiceAccountCredentials(credential_),
          impersonated_sa_));

  auto ud_options = gc::AddUniverseDomainOption(gc::ExperimentalTag{}, options);
  ASSERT_STATUS_OK(ud_options);

  auto client = kms::KeyManagementServiceClient(
      kms::MakeKeyManagementServiceConnection(*ud_options));

  for (auto kr : client.ListKeyRings(location.FullName())) {
    EXPECT_STATUS_OK(kr);
  }
}

TEST_F(IdTokenServiceAccountImpersonationTest, SAToSAImpersonationRest) {
  namespace disks = ::google::cloud::compute_disks_v1;
  // Use ADC credential
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", key_file_);

  auto ud_options = gc::AddUniverseDomainOption(gc::ExperimentalTag{});
  ASSERT_STATUS_OK(ud_options);

  auto client = disks::DisksClient(disks::MakeDisksConnectionRest(*ud_options));

  for (auto disk : client.ListDisks(project_id_, zone_id_)) {
    EXPECT_STATUS_OK(disk);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace universe_domain
}  // namespace cloud
}  // namespace google
