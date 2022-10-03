// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BIGTABLE_STUB_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BIGTABLE_STUB_FACTORY_H

#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using BaseBigtableStubFactory = std::function<std::shared_ptr<BigtableStub>(
    std::shared_ptr<grpc::Channel>)>;

/// Used in testing to create decorated mocks.
std::shared_ptr<BigtableStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BaseBigtableStubFactory const& base_factory);

/// Default function used by `DataConnectionImpl`.
std::shared_ptr<BigtableStub> CreateBigtableStub(
    google::cloud::CompletionQueue cq, Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BIGTABLE_STUB_FACTORY_H
