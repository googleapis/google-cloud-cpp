// Copyright 2024 Google LLC
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

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/discoveryengine/v1/data_store_service.proto

#include "google/cloud/discoveryengine/v1/data_store_connection_idempotency_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace discoveryengine_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;

DataStoreServiceConnectionIdempotencyPolicy::
    ~DataStoreServiceConnectionIdempotencyPolicy() = default;

std::unique_ptr<DataStoreServiceConnectionIdempotencyPolicy>
DataStoreServiceConnectionIdempotencyPolicy::clone() const {
  return std::make_unique<DataStoreServiceConnectionIdempotencyPolicy>(*this);
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::CreateDataStore(
    google::cloud::discoveryengine::v1::CreateDataStoreRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::GetDataStore(
    google::cloud::discoveryengine::v1::GetDataStoreRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::ListDataStores(
    google::cloud::discoveryengine::v1::ListDataStoresRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::DeleteDataStore(
    google::cloud::discoveryengine::v1::DeleteDataStoreRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::UpdateDataStore(
    google::cloud::discoveryengine::v1::UpdateDataStoreRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::ListOperations(
    google::longrunning::ListOperationsRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::GetOperation(
    google::longrunning::GetOperationRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency DataStoreServiceConnectionIdempotencyPolicy::CancelOperation(
    google::longrunning::CancelOperationRequest const&) {
  return Idempotency::kNonIdempotent;
}

std::unique_ptr<DataStoreServiceConnectionIdempotencyPolicy>
MakeDefaultDataStoreServiceConnectionIdempotencyPolicy() {
  return std::make_unique<DataStoreServiceConnectionIdempotencyPolicy>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace discoveryengine_v1
}  // namespace cloud
}  // namespace google
