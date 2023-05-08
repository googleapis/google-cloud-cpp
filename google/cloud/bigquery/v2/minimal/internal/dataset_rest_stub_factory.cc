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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_stub_factory.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_metadata.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<DatasetRestStub> CreateDefaultDatasetRestStub(
    Options const& opts) {
  Options local_opts = opts;
  if (!local_opts.has<UnifiedCredentialsOption>()) {
    local_opts.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials());
  }

  auto curl_rest_client = rest_internal::MakePooledRestClient(
      opts.get<EndpointOption>(), local_opts);

  std::shared_ptr<DatasetRestStub> stub =
      std::make_shared<DefaultDatasetRestStub>(std::move(curl_rest_client));

  stub = std::make_shared<DatasetMetadata>(std::move(stub));

  if (internal::Contains(local_opts.get<TracingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for REST rpc calls";
    stub = std::make_shared<DatasetLogging>(
        std::move(stub), local_opts.get<RestTracingOptionsOption>(),
        local_opts.get<TracingComponentsOption>());
  }

  return stub;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
