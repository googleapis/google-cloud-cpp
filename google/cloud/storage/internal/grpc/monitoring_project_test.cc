// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/internal/grpc/monitoring_project.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/constants.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <opentelemetry/sdk/resource/resource.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Optional;

auto KeyFileNoProject() {
  return nlohmann::json{
      {"type", "service_account"},
      {"private_key_id", "test-only-key-id"},
      {"private_key", google::cloud::storage::testing::kWellFormattedKey},
      {"client_email", "sa@invalid-test-only-project.iam.gserviceaccount.com"},
      {"client_id", "invalid-test-only-client-id"},
      {"auth_uri", "https://accounts.google.com/o/oauth2/auth"},
      {"token_uri", "https://accounts.google.com/o/oauth2/token"},
      {"auth_provider_x509_cert_url",
       "https://www.googleapis.com/oauth2/v1/certs"},
      {"client_x509_cert_url",
       "https://www.googleapis.com/robot/v1/metadata/x509/"
       "foo-email%40foo-project.iam.gserviceaccount.com"},
  };
}

auto KeyFileWithProject() {
  auto json = KeyFileNoProject();
  json["project_id"] = "project-id-credentials";
  return json;
}

TEST(MonitoringProject, Full) {
  auto resource_with_project = opentelemetry::sdk::resource::Resource::Create(
      {{"cloud.provider", "gcp"}, {"cloud.account.id", "project-id-resource"}});
  auto resource_no_project = opentelemetry::sdk::resource::Resource::Create({});
  auto credentials_with_project =
      MakeServiceAccountCredentials(KeyFileWithProject().dump());
  auto credentials_no_project =
      MakeServiceAccountCredentials(KeyFileNoProject().dump());
  auto options_with_project =
      Options{}
          .set<storage::ProjectIdOption>("project-id-options")
          .set<UnifiedCredentialsOption>(credentials_with_project);
  auto options_with_credentials_project =
      Options{}.set<UnifiedCredentialsOption>(credentials_with_project);
  auto options_no_project =
      Options{}.set<UnifiedCredentialsOption>(credentials_no_project);

  EXPECT_THAT(MonitoringProject(resource_with_project, options_with_project),
              Optional<Project>(Project("project-id-resource")));
  EXPECT_THAT(MonitoringProject(resource_no_project, options_with_project),
              Optional<Project>(Project("project-id-options")));
  EXPECT_THAT(
      MonitoringProject(resource_no_project, options_with_credentials_project),
      Optional<Project>(Project("project-id-credentials")));
  EXPECT_THAT(MonitoringProject(resource_no_project, options_no_project),
              absl::nullopt);
}

TEST(MonitoringProject, Credentials) {
  auto credentials = MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::seconds(1800));
  EXPECT_EQ(MonitoringProject(*credentials), absl::nullopt);

  credentials = MakeServiceAccountCredentials(KeyFileNoProject().dump());
  EXPECT_EQ(MonitoringProject(*credentials), absl::nullopt);

  credentials = MakeServiceAccountCredentials(KeyFileWithProject().dump());
  EXPECT_THAT(MonitoringProject(*credentials),
              Optional<Project>(Project("project-id-credentials")));
}

TEST(MonitoringProject, Resource) {
  EXPECT_EQ(MonitoringProject(opentelemetry::sdk::resource::Resource::Create(
                {{"cloud.region", "unknown"}})),
            absl::nullopt);
  EXPECT_EQ(MonitoringProject(opentelemetry::sdk::resource::Resource::Create(
                {{"cloud.account.id", "missing cloud provider"}})),
            absl::nullopt);
  EXPECT_EQ(MonitoringProject(opentelemetry::sdk::resource::Resource::Create(
                {{"cloud.provider", "missing project"}})),
            absl::nullopt);
  EXPECT_EQ(MonitoringProject(opentelemetry::sdk::resource::Resource::Create(
                {{"cloud.provider", "not-the-right-cloud"},
                 {"cloud.account.id", "test-only"}})),
            absl::nullopt);
  EXPECT_THAT(
      MonitoringProject(opentelemetry::sdk::resource::Resource::Create(
          {{"cloud.provider", "gcp"}, {"cloud.account.id", "test-only"}})),
      Optional<Project>(Project("test-only")));
}

TEST(MonitoringProject, Default) {
  ScopedEnvironment pr("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto storage_options = DefaultOptionsGrpc(Options{});
  EXPECT_EQ(MonitoringProject(storage_options),
            absl::optional<Project>(absl::nullopt));
}

TEST(MonitoringProject, WithExplicitProject) {
  // The cases where the project is set in the environment, or in both the
  // environment and the application-provided options are already tested. Here
  // we (un)set the environment only to prevent flakes.
  ScopedEnvironment pr("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto storage_options = DefaultOptionsGrpc(
      Options{}.set<storage::ProjectIdOption>("test-only-project"));
  EXPECT_THAT(MonitoringProject(storage_options),
              Optional<Project>(Project("test-only-project")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
