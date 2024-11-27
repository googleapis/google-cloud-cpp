#include "google/cloud/compute/disks/v1/disks_client.h"
#include "google/cloud/compute/disks/v1/disks_options.h"
#include "google/cloud/kms/v1/key_management_client.h"
#include "google/cloud/kms/v1/key_management_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/location.h"
#include "google/cloud/testing_util/integration_test.h"
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
    impersonated_sa_ =
        gc::internal::GetEnv("UD_IMPERSONATED_SERVICE_ACCOUNT_NAME")
            .value_or("");
    ASSERT_FALSE(impersonated_sa_.empty());
    std::string const sa_key_file =
        gc::internal::GetEnv("UD_SA_KEY_FILE").value_or("");
    ASSERT_FALSE(sa_key_file.empty());

    auto is = std::ifstream(sa_key_file);
    is.exceptions(std::ios::badbit);
    credential_ = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  }

  std::string project_id_;
  std::string zone_id_;
  std::string region_id_;
  std::string impersonated_sa_;
  std::string credential_;
};

TEST_F(DomainUniverseImpersonationTest, SAToSAImpersonationRest) {
  namespace disks = ::google::cloud::compute_disks_v1;

  gc::Options options;
  options.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeImpersonateServiceAccountCredentials(
          google::cloud::MakeServiceAccountCredentials(credential_),
          impersonated_sa_));

  auto ud_options = gc::AddUniverseDomainOption(gc::ExperimentalTag{}, options);
  if (!ud_options.ok()) throw std::move(ud_options).status();

  auto client = disks::DisksClient(disks::MakeDisksConnectionRest(*ud_options));

  for (auto disk : client.ListDisks(project_id_, zone_id_)) {
    if (!disk) throw std::move(disk).status();
  }
}

TEST_F(DomainUniverseImpersonationTest, SAToSAImpersonationGrpc) {
  namespace kms = ::google::cloud::kms_v1;

  auto const location = gc::Location(project_id_, region_id_);
  gc::Options options;
  options.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeImpersonateServiceAccountCredentials(
          google::cloud::MakeServiceAccountCredentials(credential_),
          impersonated_sa_));

  auto ud_options = gc::AddUniverseDomainOption(gc::ExperimentalTag{}, options);
  if (!ud_options.ok()) throw std::move(ud_options).status();

  auto client = kms::KeyManagementServiceClient(
      kms::MakeKeyManagementServiceConnection(*ud_options));

  for (auto kr : client.ListKeyRings(location.FullName())) {
    if (!kr) throw std::move(kr).status();
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace universe_domain
}  // namespace cloud
}  // namespace google
