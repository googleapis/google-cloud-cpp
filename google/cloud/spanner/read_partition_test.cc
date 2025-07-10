// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/read_partition.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ReadPartitionTester {
 public:
  ReadPartitionTester() = default;
  explicit ReadPartitionTester(spanner::ReadPartition partition)
      : partition_(std::move(partition)) {}
  spanner::ReadPartition const& Partition() const { return partition_; }
  std::string PartitionToken() const { return partition_.PartitionToken(); }
  std::string SessionId() const { return partition_.SessionId(); }
  std::string TransactionId() const { return partition_.TransactionId(); }
  bool RouteToLeader() const { return partition_.RouteToLeader(); }
  std::string TransactionTag() const { return partition_.TransactionTag(); }
  std::string TableName() const { return partition_.TableName(); }
  google::spanner::v1::KeySet KeySet() const { return partition_.KeySet(); }
  std::vector<std::string> ColumnNames() const {
    return partition_.ColumnNames();
  }
  bool DataBoost() const { return partition_.DataBoost(); }
  google::cloud::spanner::ReadOptions ReadOptions() const {
    return partition_.ReadOptions();
  }

 private:
  spanner::ReadPartition partition_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::spanner_internal::ReadPartitionTester;
using ::google::cloud::spanner_testing::HasSessionAndTransaction;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::Property;
using ::testing::VariantWith;

TEST(ReadPartitionTest, MakeReadPartition) {
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("txn-id");
  bool route_to_leader = false;
  std::string transaction_tag("tag");
  std::string table_name("Students");
  std::vector<std::string> column_names = {"LastName", "FirstName"};
  bool data_boost = true;
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;
  read_options.request_priority = RequestPriority::kMedium;
  read_options.request_tag = "tag";

  ReadPartitionTester actual_partition(spanner_internal::MakeReadPartition(
      transaction_id, route_to_leader, transaction_tag, session_id,
      partition_token, table_name, KeySet::All(), column_names, data_boost,
      read_options));

  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(route_to_leader, actual_partition.RouteToLeader());
  EXPECT_EQ(transaction_tag, actual_partition.TransactionTag());
  EXPECT_EQ(session_id, actual_partition.SessionId());
  EXPECT_EQ(table_name, actual_partition.TableName());
  EXPECT_THAT(actual_partition.KeySet(),
              IsProtoEqual(spanner_internal::ToProto(KeySet::All())));
  EXPECT_EQ(column_names, actual_partition.ColumnNames());
  EXPECT_TRUE(actual_partition.DataBoost());
  EXPECT_EQ(read_options, actual_partition.ReadOptions());
}

TEST(ReadPartitionTest, RegularSemantics) {
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("txn-id");
  bool route_to_leader = true;
  std::string transaction_tag("tag");
  std::string table_name("Students");
  std::vector<std::string> column_names = {"LastName", "FirstName"};
  bool data_boost = true;
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;
  read_options.request_priority = RequestPriority::kMedium;

  ReadPartition read_partition(spanner_internal::MakeReadPartition(
      transaction_id, route_to_leader, transaction_tag, session_id,
      partition_token, table_name, KeySet::All(), column_names, data_boost,
      read_options));

  EXPECT_NE(read_partition, ReadPartition());

  ReadPartition copy = read_partition;
  EXPECT_EQ(copy, read_partition);

  ReadPartition assign;
  assign = copy;
  EXPECT_EQ(assign, copy);

  ReadPartition moved = std::move(copy);
  EXPECT_EQ(moved, assign);
}

TEST(ReadPartitionTest, RouteToLeaderInequality) {
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("txn-id");
  std::string transaction_tag("tag");
  std::string table_name("Students");
  std::vector<std::string> column_names = {"LastName", "FirstName"};
  bool data_boost = true;
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;
  read_options.request_priority = RequestPriority::kMedium;

  // Verify that operator==(), which uses protobuf message comparison,
  // considers route_to_leader, which is stored in an "unknown field".
  auto read_partition_route_to_leader = spanner_internal::MakeReadPartition(
      transaction_id, /*route_to_leader=*/true, transaction_tag, session_id,
      partition_token, table_name, KeySet::All(), column_names, data_boost,
      read_options);
  auto read_partition_not_route_to_leader = spanner_internal::MakeReadPartition(
      transaction_id, /*route_to_leader=*/false, transaction_tag, session_id,
      partition_token, table_name, KeySet::All(), column_names, data_boost,
      read_options);
  EXPECT_NE(read_partition_route_to_leader, read_partition_not_route_to_leader);
}

TEST(ReadPartitionTest, SerializeDeserialize) {
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;
  read_options.request_priority = RequestPriority::kMedium;
  ReadPartitionTester expected_partition(spanner_internal::MakeReadPartition(
      "txn-id", true, "tag", "session", "token", "Students", KeySet::All(),
      std::vector<std::string>{"LastName", "FirstName"}, true, read_options));

  StatusOr<ReadPartition> partition = DeserializeReadPartition(
      *(SerializeReadPartition(expected_partition.Partition())));

  ASSERT_STATUS_OK(partition);
  ReadPartitionTester actual_partition = ReadPartitionTester(*partition);
  EXPECT_EQ(expected_partition.PartitionToken(),
            actual_partition.PartitionToken());
  EXPECT_EQ(expected_partition.TransactionId(),
            actual_partition.TransactionId());
  EXPECT_EQ(expected_partition.RouteToLeader(),
            actual_partition.RouteToLeader());
  EXPECT_EQ(expected_partition.TransactionTag(),
            actual_partition.TransactionTag());
  EXPECT_EQ(expected_partition.SessionId(), actual_partition.SessionId());
  EXPECT_EQ(expected_partition.TableName(), actual_partition.TableName());
  EXPECT_THAT(actual_partition.KeySet(),
              IsProtoEqual(expected_partition.KeySet()));
  EXPECT_EQ(expected_partition.ColumnNames(), actual_partition.ColumnNames());
  EXPECT_EQ(expected_partition.DataBoost(), actual_partition.DataBoost());
  EXPECT_EQ(expected_partition.ReadOptions(), actual_partition.ReadOptions());
}

TEST(ReadPartitionTest, FailedDeserialize) {
  std::string bad_serialized_proto("ThisIsNotTheProtoYouAreLookingFor");
  StatusOr<ReadPartition> partition =
      DeserializeReadPartition(bad_serialized_proto);
  EXPECT_THAT(partition, Not(IsOk()));
}

TEST(ReadPartitionTest, MakeReadParams) {
  std::vector<std::string> columns = {"LastName", "FirstName"};
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;
  read_options.request_priority = RequestPriority::kMedium;
  ReadPartitionTester expected_partition(spanner_internal::MakeReadPartition(
      "txn-id", true, "tag", "session", "token", "Students", KeySet::All(),
      columns, false, read_options));

  Connection::ReadParams params = spanner_internal::MakeReadParams(
      expected_partition.Partition(),
      IncludeReplicas({ReplicaSelection(ReplicaType::kReadWrite)},
                      /*auto_failover_disabled=*/true),
      google::cloud::spanner::OrderBy::kOrderByNoOrder);

  EXPECT_EQ(*params.partition_token, "token");
  EXPECT_EQ(params.read_options, read_options);
  EXPECT_FALSE(params.partition_data_boost);
  EXPECT_EQ(params.columns, columns);
  EXPECT_EQ(params.keys, KeySet::All());
  EXPECT_EQ(params.table, "Students");
  EXPECT_EQ(params.order_by, google::cloud::spanner::OrderBy::kOrderByNoOrder);
  EXPECT_THAT(params.transaction,
              HasSessionAndTransaction("session", "txn-id", true, "tag"));
  EXPECT_THAT(
      params.directed_read_option,
      VariantWith<IncludeReplicas>(AllOf(
          Property(&IncludeReplicas::replica_selections,
                   ElementsAre(ReplicaSelection(ReplicaType::kReadWrite))),
          Property(&IncludeReplicas::auto_failover_disabled, true))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
