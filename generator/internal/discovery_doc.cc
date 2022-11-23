// Copyright 2022 Google LLC
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

#include "generator/internal/discovery_to_proto.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace generator_internal {
void DoDiscovery() {
  auto constexpr kComputeDiscoveryDoc =
      "https://www.googleapis.com/discovery/v1/apis/compute/v1/rest";

  //  auto constexpr kBigQueryDiscoveryDoc =
  //      "https://bigquery.googleapis.com/discovery/v1/apis/bigquery/v2/rest";

  auto discovery_doc = GetDiscoveryDoc(kComputeDiscoveryDoc);
  if (!discovery_doc) {
    std::cerr << discovery_doc.status() << std::endl;
    return;
  }

  auto status =
      GenerateProtosFromDiscoveryDoc(*discovery_doc, "", "", "/tmp/generator");
  if (!status.ok()) {
    std::cerr << status << std::endl;
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
