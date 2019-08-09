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

#include "google/cloud/spanner/sql_partition.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class SqlPartitionTester {
 public:
  SqlPartitionTester() = default;
  explicit SqlPartitionTester(SqlPartition partition)
      : partition_(std::move(partition)) {}
  SqlStatement const& Statement() const { return partition_.sql_statement(); }
  std::string const& PartitionToken() const {
    return partition_.partition_token();
  }
  std::string const& SessionId() const { return partition_.session_id(); }
  std::string const& TransactionId() const {
    return partition_.transaction_id();
  }
  SqlPartition Partition() const { return partition_; }

 private:
  SqlPartition partition_;
};

namespace {

TEST(SqlPartitionTest, MakeSqlPartition) {
  std::string stmt("select * from foo where name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");

  SqlPartitionTester actual_partition(internal::MakeSqlPartition(
      transaction_id, session_id, partition_token, SqlStatement(stmt, params)));
  EXPECT_EQ(stmt, actual_partition.Statement().sql());
  EXPECT_EQ(params, actual_partition.Statement().params());
  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(session_id, actual_partition.SessionId());
}

TEST(SqlPartitionTest, Constructor) {
  std::string stmt("select * from foo where name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");

  SqlPartitionTester actual_partition(internal::MakeSqlPartition(
      transaction_id, session_id, partition_token, SqlStatement(stmt, params)));
  EXPECT_EQ(stmt, actual_partition.Statement().sql());
  EXPECT_EQ(params, actual_partition.Statement().params());
  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(session_id, actual_partition.SessionId());
}

TEST(SqlPartitionTester, RegularSemantics) {
  std::string stmt("select * from foo where name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");

  SqlPartition sql_partition(internal::MakeSqlPartition(
      transaction_id, session_id, partition_token, SqlStatement(stmt, params)));

  EXPECT_NE(sql_partition, SqlPartition());

  SqlPartition copy = sql_partition;
  EXPECT_EQ(copy, sql_partition);

  SqlPartition assign;
  assign = copy;
  EXPECT_EQ(assign, copy);

  SqlPartition moved = std::move(copy);
  EXPECT_EQ(moved, assign);
}

TEST(SqlPartitionTest, SerializeDeserialize) {
  SqlPartitionTester expected_partition(internal::MakeSqlPartition(
      "foo", "session", "token",
      SqlStatement("select * from foo where name = @name",
                   {{"name", Value("Bob")}})));
  StatusOr<SqlPartition> partition = DeserializeSqlPartition(
      *(SerializeSqlPartition(expected_partition.Partition())));

  ASSERT_TRUE(partition.ok());
  SqlPartitionTester actual_partition = SqlPartitionTester(*partition);
  EXPECT_EQ(expected_partition.PartitionToken(),
            actual_partition.PartitionToken());
  EXPECT_EQ(expected_partition.TransactionId(),
            actual_partition.TransactionId());
  EXPECT_EQ(expected_partition.SessionId(), actual_partition.SessionId());
  EXPECT_EQ(expected_partition.Statement().sql(),
            actual_partition.Statement().sql());
  EXPECT_EQ(expected_partition.Statement().params(),
            actual_partition.Statement().params());
}

TEST(SqlPartitionTest, FailedDeserialize) {
  std::string bad_serialized_proto("ThisIsNotTheProtoYouAreLookingFor");
  StatusOr<SqlPartition> partition =
      DeserializeSqlPartition(bad_serialized_proto);
  EXPECT_FALSE(partition.ok());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
