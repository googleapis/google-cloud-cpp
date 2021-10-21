// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESOURCE_NAMES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESOURCE_NAMES_H

#include "google/cloud/bigtable/version.h"
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

/**
 * @file
 *
 * Helper functions to create resource names.
 */

std::string InstanceName(std::string const& project_id,
                         std::string const& instance_id);

std::string TableName(std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& table_id);

std::string ClusterName(std::string const& project_id,
                        std::string const& instance_id,
                        std::string const& cluster_id);

std::string AppProfileName(std::string const& project_id,
                           std::string const& instance_id,
                           std::string const& app_profile_id);

std::string BackupName(std::string const& project_id,
                       std::string const& instance_id,
                       std::string const& cluster_id,
                       std::string const& backup_id);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESOURCE_NAMES_H
