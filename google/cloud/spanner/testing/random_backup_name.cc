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

#include "google/cloud/spanner/testing/random_backup_name.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string RandomBackupName(google::cloud::internal::DefaultPRNG& generator) {
  // A backup ID must be between 2 and 60 characters, fitting the regular
  // expression `[a-z][a-z0-9_]*[a-z0-9]`. We omit underscores from the
  // generated suffix to aid readability.
  std::size_t const max_size = 60;
  std::string const prefix = "backup-";
  auto const suffix_size = static_cast<int>(max_size - prefix.size());
  return prefix +
         google::cloud::internal::Sample(
             generator, suffix_size, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
