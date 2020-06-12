// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/read_partition.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class ReadPartitionTester {
 public:
  ReadPartitionTester() = default;
  explicit ReadPartitionTester(ReadPartition partition)
      : partition_(std::move(partition)) {}
  std::string PartitionToken() const { return partition_.PartitionToken(); }
  std::string SessionId() const { return partition_.SessionId(); }
  std::string TransactionId() const { return partition_.TransactionId(); }
  ReadPartition Partition() const { return partition_; }
  std::string TableName() const { return partition_.TableName(); }
  google::spanner::v1::KeySet KeySet() const { return partition_.KeySet(); }
  std::vector<std::string> ColumnNames() const {
    return partition_.ColumnNames();
  }
  google::cloud::spanner::ReadOptions ReadOptions() const {
    return partition_.ReadOptions();
  }

 private:
  ReadPartition partition_;
};

namespace {

using ::google::cloud::spanner_testing::HasSessionAndTransactionId;

TEST(ReadPartitionTest, MakeReadPartition) {
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");
  std::string table_name("Students");
  std::vector<std::string> column_names = {"LastName", "FirstName"};

  ReadPartitionTester actual_partition(
      internal::MakeReadPartition(transaction_id, session_id, partition_token,
                                  table_name, KeySet::All(), column_names));

  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(session_id, actual_partition.SessionId());
  EXPECT_EQ(table_name, actual_partition.TableName());
  EXPECT_THAT(actual_partition.KeySet(),
              spanner_testing::IsProtoEqual(internal::ToProto(KeySet::All())));
  EXPECT_EQ(column_names, actual_partition.ColumnNames());
}

TEST(ReadPartitionTest, Constructor) {
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");
  std::string table_name("Students");
  std::vector<std::string> column_names = {"LastName", "FirstName"};
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;

  ReadPartitionTester actual_partition(internal::MakeReadPartition(
      transaction_id, session_id, partition_token, table_name, KeySet::All(),
      column_names, read_options));
  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(session_id, actual_partition.SessionId());
  EXPECT_EQ(table_name, actual_partition.TableName());
  EXPECT_THAT(actual_partition.KeySet(),
              spanner_testing::IsProtoEqual(internal::ToProto(KeySet::All())));
  EXPECT_EQ(column_names, actual_partition.ColumnNames());
  EXPECT_EQ(read_options, actual_partition.ReadOptions());
}

TEST(ReadPartitionTest, RegularSemantics) {
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");
  std::string table_name("Students");
  std::vector<std::string> column_names = {"LastName", "FirstName"};
  ReadOptions read_options;
  read_options.index_name = "secondary";
  read_options.limit = 42;

  ReadPartition read_partition(internal::MakeReadPartition(
      transaction_id, session_id, partition_token, table_name, KeySet::All(),
      column_names, read_options));

  EXPECT_NE(read_partition, ReadPartition());

  ReadPartition copy = read_partition;
  EXPECT_EQ(copy, read_partition);

  ReadPartition assign;
  assign = copy;
  EXPECT_EQ(assign, copy);

  ReadPartition moved = std::move(copy);
  EXPECT_EQ(moved, assign);
}

TEST(ReadPartitionTest, SerializeDeserialize) {
  ReadPartitionTester expected_partition(internal::MakeReadPartition(
      "foo", "session", "token", "Students", KeySet::All(),
      std::vector<std::string>{"LastName", "FirstName"}));

  StatusOr<ReadPartition> partition = DeserializeReadPartition(
      *(SerializeReadPartition(expected_partition.Partition())));

  ASSERT_STATUS_OK(partition);
  ReadPartitionTester actual_partition = ReadPartitionTester(*partition);
  EXPECT_EQ(expected_partition.PartitionToken(),
            actual_partition.PartitionToken());
  EXPECT_EQ(expected_partition.TransactionId(),
            actual_partition.TransactionId());
  EXPECT_EQ(expected_partition.SessionId(), actual_partition.SessionId());
  EXPECT_EQ(expected_partition.TableName(), actual_partition.TableName());
  EXPECT_THAT(actual_partition.KeySet(),
              spanner_testing::IsProtoEqual(expected_partition.KeySet()));
  EXPECT_EQ(expected_partition.ColumnNames(), actual_partition.ColumnNames());
  EXPECT_EQ(expected_partition.ReadOptions(), actual_partition.ReadOptions());
}

TEST(ReadPartitionTest, FailedDeserialize) {
  std::string bad_serialized_proto("ThisIsNotTheProtoYouAreLookingFor");
  StatusOr<ReadPartition> partition =
      DeserializeReadPartition(bad_serialized_proto);
  EXPECT_FALSE(partition.ok());
}

TEST(ReadPartitionTest, MakeReadParams) {
  std::vector<std::string> columns = {"LastName", "FirstName"};
  ReadPartitionTester expected_partition(internal::MakeReadPartition(
      "foo", "session", "token", "Students", KeySet::All(), columns));

  Connection::ReadParams params =
      internal::MakeReadParams(expected_partition.Partition());

  EXPECT_EQ(*params.partition_token, "token");
  EXPECT_EQ(params.keys, KeySet::All());
  EXPECT_EQ(params.columns, columns);
  EXPECT_EQ(params.table, "Students");
  EXPECT_THAT(params.transaction, HasSessionAndTransactionId("session", "foo"));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
