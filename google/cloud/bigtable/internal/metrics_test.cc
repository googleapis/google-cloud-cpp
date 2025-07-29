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
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::SetServerMetadata;

using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(LabelMap, IntoLabelMap) {
  ResourceLabels r{"my-project", "my-instance", "my-table", "my-cluster",
                   "my-zone"};
  DataLabels d{"my-method",     "my-streaming",   "my-client-name",
               "my-client-uid", "my-app-profile", "my-status"};

  auto label_map = IntoLabelMap(r, d);

  EXPECT_THAT(
      label_map,
      UnorderedElementsAre(
          Pair("project_id", "my-project"), Pair("instance", "my-instance"),
          Pair("table", "my-table"), Pair("cluster", "my-cluster"),
          Pair("zone", "my-zone"), Pair("method", "my-method"),
          Pair("streaming", "my-streaming"),
          Pair("client_name", "my-client-name"),
          Pair("client_uid", "my-client-uid"),
          Pair("app_profile", "my-app-profile"), Pair("status", "my-status")));
}

TEST(GetResponseParamsFromMetadata, NonEmptyHeader) {
  google::bigtable::v2::ResponseParams expected_response_params;
  expected_response_params.set_cluster_id("my-cluster");
  expected_response_params.set_zone_id("my-zone");
  grpc::ClientContext client_context;
  RpcMetadata server_metadata;
  server_metadata.trailers.emplace(
      "x-goog-ext-425905942-bin", expected_response_params.SerializeAsString());
  SetServerMetadata(client_context, server_metadata);

  auto result = GetResponseParamsFromTrailingMetadata(client_context);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->cluster_id(), Eq("my-cluster"));
  EXPECT_THAT(result->zone_id(), Eq("my-zone"));
}

TEST(GetResponseParamsFromMetadata, EmptyHeader) {
  grpc::ClientContext client_context;
  RpcMetadata server_metadata;
  SetServerMetadata(client_context, server_metadata);

  auto result = GetResponseParamsFromTrailingMetadata(client_context);
  EXPECT_FALSE(result);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
