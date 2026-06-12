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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_STUB_MANAGER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_STUB_MANAGER_H

#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/version.h"
#include "absl/container/flat_hash_map.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class provides support for operating with a single BigtableStub which is
// not bound to any specific Bigtable instance, or with a collection of
// BigtableStubs of which each BigtableStub is bound to a specific instance.
class StubManager {
 public:
  enum class Priming { kNoPriming, kSynchronousPriming };

  // This function must not send any RPCs as it is called while a lock is held.
  // This means channels cannot be primed as part of construction, but instead
  // must be scheduled to be primed asynchronously.
  using StubCreationFn = std::function<std::shared_ptr<BigtableStub>(
      std::string_view instance_name, Priming priming)>;

  explicit StubManager(std::shared_ptr<BigtableStub> stub);

  StubManager(absl::flat_hash_map<std::string, std::shared_ptr<BigtableStub>>
                  affinity_stubs,
              StubCreationFn stub_creation_fn);

  std::shared_ptr<BigtableStub> GetStub(std::string_view instance_name);

 private:
  std::mutex mu_;
  StubCreationFn stub_creation_fn_;
  std::shared_ptr<BigtableStub> stub_;
  absl::flat_hash_map<std::string, std::shared_ptr<BigtableStub>>
      affinity_stubs_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_STUB_MANAGER_H
