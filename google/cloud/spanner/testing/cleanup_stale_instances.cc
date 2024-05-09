// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/testing/cleanup_stale_instances.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/random.h"
#include <chrono>
#include <regex>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

// The cutoff date for resources considered stale.
std::string CutoffDate() {
  auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24);
  return internal::FormatRfc3339(cutoff_time).substr(0, 10);  // YYYY-MM-DD
}

}  // namespace

Status CleanupStaleInstances(
    Project const& project,
    spanner_admin::InstanceAdminClient instance_admin_client,
    spanner_admin::DatabaseAdminClient database_admin_client) {
  std::regex name_regex(R"(projects/.+/instances/)"
                        R"(temporary-instance-(\d{4}-\d{2}-\d{2})-.+)");

  // Make sure we're using a regex that matches a random instance name.
  if (name_regex.mark_count() != 1) {
    return internal::InternalError(
        "Instance regex must have a single capture group", GCP_ERROR_INFO());
  }
  auto generator = internal::MakeDefaultPRNG();
  auto random_id = spanner_testing::RandomInstanceName(generator);
  auto full_name = spanner::Instance(project, random_id).FullName();
  std::smatch m;
  if (!std::regex_match(full_name, m, name_regex)) {
    return internal::InternalError(
        "Instance regex does not match a random instance name",
        GCP_ERROR_INFO());
  }

  auto cutoff_date = CutoffDate();
  std::vector<std::string> instances;
  for (auto const& instance :
       instance_admin_client.ListInstances(project.FullName())) {
    if (!instance) break;
    auto name = instance->name();
    std::smatch m;
    if (std::regex_match(name, m, name_regex)) {
      auto date_str = m[1];
      if (date_str < cutoff_date) {
        instances.push_back(name);
      }
    }
  }

  // We ignore failures here.
  for (auto const& instance : instances) {
    for (auto const& backup : database_admin_client.ListBackups(instance)) {
      database_admin_client.DeleteBackup(backup->name());
    }
    instance_admin_client.DeleteInstance(instance);
  }
  return Status();
}

Status CleanupStaleInstanceConfigs(
    Project const& project,
    spanner_admin::InstanceAdminClient instance_admin_client) {
  std::regex name_regex(R"(projects/.+/instanceConfigs/)"
                        R"(custom-temporary-config-(\d{4}-\d{2}-\d{2})-.+)");

  // Make sure we're using a regex that matches a random config name.
  if (name_regex.mark_count() != 1) {
    return internal::InternalError(
        "Config regex must have a single capture group", GCP_ERROR_INFO());
  }
  auto generator = internal::MakeDefaultPRNG();
  auto random_id = spanner_testing::RandomInstanceConfigName(generator);
  auto full_name = project.FullName() + "/instanceConfigs/" + random_id;
  std::smatch m;
  if (!std::regex_match(full_name, m, name_regex)) {
    return internal::InternalError(
        "Config regex does not match a random config name", GCP_ERROR_INFO());
  }

  auto cutoff_date = CutoffDate();
  std::vector<std::string> configs;
  for (auto const& config :
       instance_admin_client.ListInstanceConfigs(project.FullName())) {
    if (!config) break;
    auto name = config->name();
    std::smatch m;
    if (std::regex_match(name, m, name_regex)) {
      auto date_str = m[1];
      if (date_str < cutoff_date) {
        configs.push_back(name);
      }
    }
  }

  // We ignore failures here.
  for (auto const& config : configs) {
    instance_admin_client.DeleteInstanceConfig(config);
  }
  return Status();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
