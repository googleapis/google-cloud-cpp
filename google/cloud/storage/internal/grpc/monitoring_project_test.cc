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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/grpc/monitoring_project.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/resource/resource.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Optional;

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

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
