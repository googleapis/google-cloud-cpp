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

#include "google/cloud/opentelemetry/internal/monitored_resource.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
namespace sc = opentelemetry::sdk::resource::SemanticConventions;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(AsString, Simple) {
  struct TestCase {
    opentelemetry::sdk::common::OwnedAttributeValue value;
    std::string result;
  };
  std::vector<TestCase> cases = {
      {true, "true"},    {false, "false"},
      {int32_t{1}, "1"}, {uint32_t{2}, "2"},
      {int64_t{3}, "3"}, {uint64_t{4}, "4"},
      {5.6, "5.6"},      {std::string{"value"}, "value"},
  };

  for (auto const& c : cases) {
    EXPECT_EQ(c.result, AsString(c.value));
  }
}

TEST(AsString, VectorsAreJoined) {
  struct TestCase {
    opentelemetry::sdk::common::OwnedAttributeValue value;
    std::string result;
  };

  std::vector<std::string> const v = {"value1", "value2"};
  std::vector<TestCase> cases = {
      {std::vector<bool>{{true, false}}, "[true, false]"},
      {std::vector<int32_t>{{1, 2}}, "[1, 2]"},
      {std::vector<uint32_t>{{3, 4}}, "[3, 4]"},
      {std::vector<int64_t>{{5, 6}}, "[5, 6]"},
      {std::vector<uint64_t>{{7, 8}}, "[7, 8]"},
      {std::vector<uint8_t>{{9, 10}}, "[9, 10]"},
      {std::vector<double>{{1.1, 2.2}}, "[1.1, 2.2]"},
      {v, "[value1, value2]"},
  };

  for (auto const& c : cases) {
    EXPECT_EQ(c.result, AsString(c.value));
  }
}

TEST(MonitoredResource, GceInstance) {
  auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
      {sc::kCloudPlatform, "gcp_compute_engine"},
      {sc::kHostId, "1020304050607080900"},
      {sc::kCloudAvailabilityZone, "us-central1-a"},
  };

  auto mr = ToMonitoredResource(attributes);
  EXPECT_EQ(mr.type, "gce_instance");
  EXPECT_THAT(mr.labels,
              UnorderedElementsAre(Pair("zone", "us-central1-a"),
                                   Pair("instance_id", "1020304050607080900")));
}

TEST(MonitoredResource, K8sContainer) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudPlatform, "gcp_kubernetes_engine"},
        {sc::kK8sClusterName, "test-cluster"},
        {sc::kK8sNamespaceName, "test-namespace"},
        {sc::kK8sPodName, "test-pod"},
        {sc::kK8sContainerName, "test-container"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "k8s_container");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("cluster_name", "test-cluster"),
                                     Pair("namespace_name", "test-namespace"),
                                     Pair("pod_name", "test-pod"),
                                     Pair("container_name", "test-container")));
  }
}

TEST(MonitoredResource, K8sPod) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudPlatform, "gcp_kubernetes_engine"},
        {sc::kK8sClusterName, "test-cluster"},
        {sc::kK8sNamespaceName, "test-namespace"},
        {sc::kK8sPodName, "test-pod"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "k8s_pod");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("cluster_name", "test-cluster"),
                                     Pair("namespace_name", "test-namespace"),
                                     Pair("pod_name", "test-pod")));
  }
}

TEST(MonitoredResource, K8sNode) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudPlatform, "gcp_kubernetes_engine"},
        {sc::kK8sClusterName, "test-cluster"},
        {sc::kK8sNodeName, "test-node"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "k8s_node");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("cluster_name", "test-cluster"),
                                     Pair("node_name", "test-node")));
  }
}

TEST(MonitoredResource, K8sCluster) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudPlatform, "gcp_kubernetes_engine"},
        {sc::kK8sClusterName, "test-cluster"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "k8s_cluster");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("cluster_name", "test-cluster")));
  }
}

TEST(MonitoredResource, GaeInstance) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudPlatform, "gcp_app_engine"},
        {sc::kFaasName, "test-module"},
        {sc::kFaasVersion, "test-version"},
        {sc::kFaasInstance, "test-instance"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "gae_instance");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("module_id", "test-module"),
                                     Pair("version_id", "test-version"),
                                     Pair("instance_id", "test-instance")));
  }
}

TEST(MonitoredResource, AwsEc2Instance) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_region;
  };
  auto tests = std::vector<TestCase>{
      {"test-zone", "test-region", "test-zone"},
      {"test-zone", absl::nullopt, "test-zone"},
      {absl::nullopt, "test-region", "test-region"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudPlatform, "aws_ec2"},
        {sc::kHostId, "test-instance"},
        {sc::kCloudAccountId, "test-account"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "aws_ec2_instance");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("instance_id", "test-instance"),
                                     Pair("region", test.expected_region),
                                     Pair("aws_account", "test-account")));
  }
}

TEST(MonitoredResource, GenericTaskFaas) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
      {absl::nullopt, absl::nullopt, "global"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kFaasName, "faas-name"},
        {sc::kFaasInstance, "faas-instance"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "generic_task");
    EXPECT_THAT(mr.labels, UnorderedElementsAre(
                               Pair("location", test.expected_location),
                               // Verify fallback to empty string.
                               Pair("namespace", ""), Pair("job", "faas-name"),
                               Pair("task_id", "faas-instance")));
  }
}

TEST(MonitoredResource, GenericTaskService) {
  struct TestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto tests = std::vector<TestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
      {absl::nullopt, absl::nullopt, "global"},
  };
  for (auto const& test : tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kServiceNamespace, "test-namespace"},
        {sc::kServiceName, "test-name"},
        {sc::kServiceInstanceId, "test-instance"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "generic_task");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("namespace", "test-namespace"),
                                     Pair("job", "test-name"),
                                     Pair("task_id", "test-instance")));
  }
}

TEST(MonitoredResource, GenericNode) {
  struct LocationTestCase {
    absl::optional<std::string> zone;
    absl::optional<std::string> region;
    std::string expected_location;
  };
  auto location_tests = std::vector<LocationTestCase>{
      {"us-central1-a", "us-central1", "us-central1-a"},
      {"us-central1-a", absl::nullopt, "us-central1-a"},
      {absl::nullopt, "us-central1", "us-central1"},
      {absl::nullopt, absl::nullopt, "global"},
  };
  for (auto const& test : location_tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kServiceNamespace, "test-namespace"},
        {sc::kHostId, "test-instance"},
    };
    if (test.zone) attributes[sc::kCloudAvailabilityZone] = *test.zone;
    if (test.region) attributes[sc::kCloudRegion] = *test.region;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "generic_node");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", test.expected_location),
                                     Pair("namespace", "test-namespace"),
                                     Pair("node_id", "test-instance")));
  }

  struct NodeIDTestCase {
    absl::optional<std::string> host_id;
    std::string expected_node_id;
  };
  auto node_id_tests = std::vector<NodeIDTestCase>{
      {"test-instance", "test-instance"},
      {absl::nullopt, "test-name"},
  };
  for (auto const& test : node_id_tests) {
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{
        {sc::kCloudAvailabilityZone, "us-central1-a"},
        {sc::kCloudRegion, "us-central1"},
        {sc::kServiceNamespace, "test-namespace"},
        {sc::kHostName, "test-name"},
    };
    if (test.host_id) attributes[sc::kHostId] = *test.host_id;

    auto mr = ToMonitoredResource(attributes);
    EXPECT_EQ(mr.type, "generic_node");
    EXPECT_THAT(mr.labels,
                UnorderedElementsAre(Pair("location", "us-central1-a"),
                                     Pair("namespace", "test-namespace"),
                                     Pair("node_id", test.expected_node_id)));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
