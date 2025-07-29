// Copyright 2025 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/version.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

LabelMap IntoLabelMap(ResourceLabels const& r, DataLabels const& d) {
  return {
      {"project_id", r.project_id},
      {"instance", r.instance},
      {"table", r.table},
      {"cluster", r.cluster},
      {"zone", r.zone},
      {"method", d.method},
      {"streaming", d.streaming},
      {"client_name", d.client_name},
      {"client_uid", d.client_uid},
      {"app_profile", d.app_profile},
      {"status", d.status},
  };
}

absl::optional<google::bigtable::v2::ResponseParams>
GetResponseParamsFromTrailingMetadata(
    grpc::ClientContext const& client_context) {
  auto metadata = client_context.GetServerTrailingMetadata();
  auto iter = metadata.find("x-goog-ext-425905942-bin");
  if (iter == metadata.end()) return absl::nullopt;
  google::bigtable::v2::ResponseParams p;
  // The value for this key should always be the same in a response, so we
  // return the first value we find.
  std::string value{iter->second.data(), iter->second.size()};
  if (p.ParseFromString(value)) return p;
  return absl::nullopt;
}

Metric::~Metric() = default;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
