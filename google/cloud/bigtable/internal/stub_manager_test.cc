// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/internal/stub_manager.h"
#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/table_resource.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::IsOk;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::StartsWith;

TEST(StubManagerTest, NoAffinity) {
  bigtable::InstanceResource instance(Project("my-project"), "my-instance");
  bigtable::TableResource table(instance, "my-table");
  std::string const expected_table_name = table.FullName();
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([&](grpc::ClientContext&, Options const&,
                    google::bigtable::v2::MutateRowRequest const& request) {
        EXPECT_THAT(request.table_name(), Eq(expected_table_name));
        return google::bigtable::v2::MutateRowResponse{};
      });

  StubManager manager(mock);

  auto stub = manager.GetStub({});
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest request;
  request.set_table_name(expected_table_name);
  auto result = stub->MutateRow(context, {}, request);
  EXPECT_THAT(result, IsOk());
}

TEST(StubManagerTest, AffinityToExistingInstance) {
  bigtable::InstanceResource instance(Project("my-project"), "a");
  bigtable::TableResource table(instance, "my-table");
  std::string const expected_table_name = table.FullName();

  auto mock_stub = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock_stub, MutateRow)
      .WillOnce([instance_name = instance.FullName()](
                    grpc::ClientContext&, Options const&,
                    google::bigtable::v2::MutateRowRequest const& request) {
        EXPECT_THAT(request.table_name(), StartsWith(instance_name));
        return google::bigtable::v2::MutateRowResponse{};
      });

  absl::flat_hash_map<std::string, std::shared_ptr<BigtableStub>>
      affinity_stubs;
  affinity_stubs[instance.FullName()] = mock_stub;

  MockFunction<std::shared_ptr<BigtableStub>(std::string_view,
                                             StubManager::Priming)>
      stub_creation_fn;
  EXPECT_CALL(stub_creation_fn, Call).Times(0);

  StubManager manager(affinity_stubs, stub_creation_fn.AsStdFunction());

  auto stub = manager.GetStub(instance.FullName());
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest request;
  request.set_table_name(expected_table_name);
  auto result = stub->MutateRow(context, {}, request);
  EXPECT_THAT(result, IsOk());
}

TEST(StubManagerTest, AffinityToMissingInstance) {
  bigtable::InstanceResource instance_a(Project("my-project"), "a");
  bigtable::TableResource table_a(instance_a, "my-table");
  auto mock_stub_a = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock_stub_a, MutateRow).Times(0);
  absl::flat_hash_map<std::string, std::shared_ptr<BigtableStub>>
      affinity_stubs;
  affinity_stubs[instance_a.FullName()] = mock_stub_a;

  auto stub_creation_fn = [](std::string_view instance_name,
                             StubManager::Priming) {
    auto mock_stub = std::make_shared<MockBigtableStub>();
    EXPECT_CALL(*mock_stub, MutateRow)
        .WillOnce([instance_name = std::string{instance_name}](
                      grpc::ClientContext&, Options const&,
                      google::bigtable::v2::MutateRowRequest const& request) {
          EXPECT_THAT(request.table_name(), StartsWith(instance_name));
          return google::bigtable::v2::MutateRowResponse{};
        });
    return mock_stub;
  };

  StubManager manager(affinity_stubs, stub_creation_fn);

  bigtable::InstanceResource instance_b(Project("my-project"), "b");
  bigtable::TableResource table_b(instance_b, "my-table");
  std::string const expected_table_name = table_b.FullName();
  auto stub = manager.GetStub(instance_b.FullName());
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest request;
  request.set_table_name(expected_table_name);
  auto result = stub->MutateRow(context, {}, request);
  EXPECT_THAT(result, IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
