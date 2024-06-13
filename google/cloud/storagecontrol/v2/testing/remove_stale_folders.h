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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGECONTROL_V2_TESTING_REMOVE_STALE_FOLDERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGECONTROL_V2_TESTING_REMOVE_STALE_FOLDERS_H

#include "google/cloud/storagecontrol/v2/storage_control_client.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/status.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace storagecontrol_v2 {
namespace testing {

/**
 * Remove stale folders created for examples.
 *
 * The examples create folders in the production
 * environment. While these programs are supposed to clean after themselves,
 * they might crash or otherwise fail to delete any folders they create. These
 * folders can accumulate and cause future tests to fail. To prevent
 * these problems we delete any folder that match the pattern of these randomly
 * created folders, as long as the folder was created more than 48 hours ago.
 *
 * @param client
 * @param prefix
 * @param created_time_limit
 * @return Status
 */
Status RemoveStaleFolders(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::string const& bucket_name, std::string const& prefix,
    std::chrono::system_clock::time_point created_time_limit);

}  // namespace testing
}  // namespace storagecontrol_v2
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGECONTROL_V2_TESTING_REMOVE_STALE_FOLDERS_H
