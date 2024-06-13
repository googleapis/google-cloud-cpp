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

#include "google/cloud/storagecontrol/v2/testing/remove_stale_folders.h"
#include <regex>

namespace google {
namespace cloud {
namespace storagecontrol_v2 {
namespace testing {

Status RemoveStaleFolders(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::string const& bucket_name, std::string const& prefix,
    std::chrono::system_clock::time_point created_time_limit) {
  std::regex re(prefix + R"re(-[a-z]{32})re");
  auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
  for (auto folder : client.ListFolders(parent)) {
    if (!folder) return std::move(folder).status();
    if (!std::regex_match(folder->name(), re)) continue;
    auto const create_time =
        google::cloud::internal::ToChronoTimePoint(folder->create_time());
    if (create_time > created_time_limit) continue;
    (void)client.DeleteFolder(folder->name());
  }
  return {};
}

}  // namespace testing
}  // namespace storagecontrol_v2
}  // namespace cloud
}  // namespace google
