// Copyright 2023 Google LLC
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

// Implementation of internal interface for Bigquery V2 Dataset resource.

#include "google/cloud/bigquery/v2/minimal/internal/dataset_idempotency_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;

DatasetIdempotencyPolicy::~DatasetIdempotencyPolicy() = default;

std::unique_ptr<DatasetIdempotencyPolicy> DatasetIdempotencyPolicy::clone()
    const {
  return std::make_unique<DatasetIdempotencyPolicy>(*this);
}

Idempotency DatasetIdempotencyPolicy::GetDataset(GetDatasetRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency DatasetIdempotencyPolicy::ListDatasets(ListDatasetsRequest const&) {
  return Idempotency::kIdempotent;
}

std::unique_ptr<DatasetIdempotencyPolicy>
MakeDefaultDatasetIdempotencyPolicy() {
  return std::make_unique<DatasetIdempotencyPolicy>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
