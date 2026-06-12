// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/stub_manager.h"
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StubManager::StubManager(std::shared_ptr<BigtableStub> stub)
    : stub_(std::move(stub)) {}

StubManager::StubManager(
    absl::flat_hash_map<std::string, std::shared_ptr<BigtableStub>>
        affinity_stubs,
    StubCreationFn stub_creation_fn)
    : stub_creation_fn_(std::move(stub_creation_fn)),
      affinity_stubs_(std::move(affinity_stubs)) {}

std::shared_ptr<BigtableStub> StubManager::GetStub(
    std::string_view instance_name) {
  if (stub_) return stub_;

  std::scoped_lock lk(mu_);
  if (auto iter = affinity_stubs_.find(instance_name);
      iter != affinity_stubs_.end()) {
    return iter->second;
  }
  auto inserted = affinity_stubs_.emplace(
      std::string{instance_name},
      stub_creation_fn_(instance_name, Priming::kNoPriming));
  return inserted.first->second;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
