// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/connection_factory.h"
#include "google/cloud/storage/internal/connection_impl.h"
#include "google/cloud/storage/internal/generic_stub_adapter.h"
#include "google/cloud/storage/internal/generic_stub_factory.h"
#include "google/cloud/storage/internal/tracing_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<storage::internal::StorageConnection> DecorateConnection(
    Options const& opts,
    std::shared_ptr<storage::internal::StorageConnection> connection) {
  auto stub = MakeGenericStubAdapter(std::move(connection));
  return MakeStorageConnection(opts, std::move(stub));
}

std::shared_ptr<storage::internal::StorageConnection> MakeStorageConnection(
    Options const& opts, std::unique_ptr<storage_internal::GenericStub> stub) {
  std::shared_ptr<storage::internal::StorageConnection> connection =
      storage::internal::StorageConnectionImpl::Create(std::move(stub), opts);
  if (google::cloud::internal::TracingEnabled(opts)) {
    connection = storage_internal::MakeTracingClient(std::move(connection));
  }
  return connection;
}

std::shared_ptr<storage::internal::StorageConnection> MakeStorageConnection(
    Options const& opts) {
  return MakeStorageConnection(opts, MakeDefaultStorageStub(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
