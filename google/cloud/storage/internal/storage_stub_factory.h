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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_FACTORY_H

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using BaseStorageStubFactory =
    std::function<std::shared_ptr<StorageStub>(std::shared_ptr<grpc::Channel>)>;

std::shared_ptr<StorageStub> CreateStorageStubRoundRobin(
    Options const& options,
    std::function<std::shared_ptr<StorageStub>(int)> child_factory);

/// Used in testing to create decorated mocks.
std::shared_ptr<StorageStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BaseStorageStubFactory const& base_factory);

/// Default function used by the `GrpcClient`.
std::shared_ptr<StorageStub> CreateStorageStub(
    google::cloud::CompletionQueue cq, Options const& options);

std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub>
CreateStorageIamStub(google::cloud::CompletionQueue cq, Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_FACTORY_H
