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

#include "google/cloud/bigtable/internal/bigtable_random_two_least_used_decorator.h"
#include "google/cloud/bigtable/internal/dynamic_channel_pool.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::testing::MockFunction;

class BigtableRandomTwoLeastUsedTest : public ::testing::Test {
 protected:
  BigtableRandomTwoLeastUsedTest() {
    mock_stub_ = std::make_shared<MockBigtableStub>();

    fake_cq_impl_ = std::make_shared<FakeCompletionQueueImpl>();
    cq_ = CompletionQueue(fake_cq_impl_);
    auto instance_name =
        bigtable::InstanceResource(Project("my-project"), "my-instance")
            .FullName();
    bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy;
    auto refresh_state = std::make_shared<ConnectionRefreshState>(
        fake_cq_impl_, std::chrono::milliseconds(1),
        std::chrono::milliseconds(10));

    std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
    channels.push_back(
        std::make_shared<ChannelUsage<BigtableStub>>(mock_stub_, 0));

    pool_ = DynamicChannelPool<BigtableStub>::Create(
        instance_name, cq_, channels, refresh_state,
        stub_factory_fn_.AsStdFunction(), sizing_policy);
  }

  ~BigtableRandomTwoLeastUsedTest() override {
    fake_cq_impl_->SimulateCompletion(false);
  }

  std::shared_ptr<MockBigtableStub> mock_stub_;
  std::shared_ptr<DynamicChannelPool<BigtableStub>> pool_;
  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn_;
  CompletionQueue cq_;
  std::shared_ptr<FakeCompletionQueueImpl> fake_cq_impl_;
};

TEST_F(BigtableRandomTwoLeastUsedTest, ReadRows) {
  EXPECT_CALL(*mock_stub_, ReadRows).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::ReadRowsRequest request;
  auto response = decorator->ReadRows(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, SampleRowKeys) {
  EXPECT_CALL(*mock_stub_, SampleRowKeys).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::SampleRowKeysRequest request;
  auto response = decorator->SampleRowKeys(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, MutateRow) {
  EXPECT_CALL(*mock_stub_, MutateRow).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  grpc::ClientContext client_context;
  google::bigtable::v2::MutateRowRequest request;
  auto response = decorator->MutateRow(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, MutateRows) {
  EXPECT_CALL(*mock_stub_, MutateRows).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::MutateRowsRequest request;
  auto response = decorator->MutateRows(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, CheckAndMutateRow) {
  EXPECT_CALL(*mock_stub_, CheckAndMutateRow).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  grpc::ClientContext client_context;
  google::bigtable::v2::CheckAndMutateRowRequest request;
  auto response = decorator->CheckAndMutateRow(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, PingAndWarm) {
  EXPECT_CALL(*mock_stub_, PingAndWarm).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  grpc::ClientContext client_context;
  google::bigtable::v2::PingAndWarmRequest request;
  auto response = decorator->PingAndWarm(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, ReadModifyWriteRow) {
  EXPECT_CALL(*mock_stub_, ReadModifyWriteRow).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  grpc::ClientContext client_context;
  google::bigtable::v2::ReadModifyWriteRowRequest request;
  auto response = decorator->ReadModifyWriteRow(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, PrepareQuery) {
  EXPECT_CALL(*mock_stub_, PrepareQuery).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  grpc::ClientContext client_context;
  google::bigtable::v2::PrepareQueryRequest request;
  auto response = decorator->PrepareQuery(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, ExecuteQuery) {
  EXPECT_CALL(*mock_stub_, ExecuteQuery).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::ExecuteQueryRequest request;
  auto response = decorator->ExecuteQuery(client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncReadRows) {
  EXPECT_CALL(*mock_stub_, AsyncReadRows).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::ReadRowsRequest request;
  auto response = decorator->AsyncReadRows(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncSampleRowKeys) {
  EXPECT_CALL(*mock_stub_, AsyncSampleRowKeys).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::SampleRowKeysRequest request;
  auto response =
      decorator->AsyncSampleRowKeys(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncMutateRow) {
  EXPECT_CALL(*mock_stub_, AsyncMutateRow).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::MutateRowRequest request;
  auto response = decorator->AsyncMutateRow(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncMutateRows) {
  EXPECT_CALL(*mock_stub_, AsyncMutateRows).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::MutateRowsRequest request;
  auto response = decorator->AsyncMutateRows(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncCheckAndMutateRow) {
  EXPECT_CALL(*mock_stub_, AsyncCheckAndMutateRow).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::CheckAndMutateRowRequest request;
  auto response =
      decorator->AsyncCheckAndMutateRow(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncPingAndWarm) {
  EXPECT_CALL(*mock_stub_, AsyncPingAndWarm).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::PingAndWarmRequest request;
  auto response = decorator->AsyncPingAndWarm(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncReadModifyWriteRow) {
  EXPECT_CALL(*mock_stub_, AsyncReadModifyWriteRow).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::ReadModifyWriteRowRequest request;
  auto response =
      decorator->AsyncReadModifyWriteRow(cq_, client_context, {}, request);
}

TEST_F(BigtableRandomTwoLeastUsedTest, AsyncPrepareQuery) {
  EXPECT_CALL(*mock_stub_, AsyncPrepareQuery).Times(1);
  auto decorator = std::make_shared<BigtableRandomTwoLeastUsed>(pool_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  google::bigtable::v2::PrepareQueryRequest request;
  auto response =
      decorator->AsyncPrepareQuery(cq_, client_context, {}, request);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
