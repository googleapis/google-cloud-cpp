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

#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/testing/matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class QueryPartitionTester {
 public:
  QueryPartitionTester() = default;
  explicit QueryPartitionTester(QueryPartition partition)
      : partition_(std::move(partition)) {}
  SqlStatement const& Statement() const { return partition_.sql_statement(); }
  std::string const& PartitionToken() const {
    return partition_.partition_token();
  }
  std::string const& SessionId() const { return partition_.session_id(); }
  std::string const& TransactionId() const {
    return partition_.transaction_id();
  }
  QueryPartition Partition() const { return partition_; }

 private:
  QueryPartition partition_;
};

namespace {

using ::google::cloud::spanner_testing::HasSessionAndTransactionId;

TEST(QueryPartitionTest, MakeQueryPartition) {
  std::string stmt("select * from foo where name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");

  QueryPartitionTester actual_partition(internal::MakeQueryPartition(
      transaction_id, session_id, partition_token, SqlStatement(stmt, params)));
  EXPECT_EQ(stmt, actual_partition.Statement().sql());
  EXPECT_EQ(params, actual_partition.Statement().params());
  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(session_id, actual_partition.SessionId());
}

TEST(QueryPartitionTest, Constructor) {
  std::string stmt("select * from foo where name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");

  QueryPartitionTester actual_partition(internal::MakeQueryPartition(
      transaction_id, session_id, partition_token, SqlStatement(stmt, params)));
  EXPECT_EQ(stmt, actual_partition.Statement().sql());
  EXPECT_EQ(params, actual_partition.Statement().params());
  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(session_id, actual_partition.SessionId());
}

TEST(QueryPartitionTester, RegularSemantics) {
  std::string stmt("select * from foo where name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("foo");

  QueryPartition query_partition(internal::MakeQueryPartition(
      transaction_id, session_id, partition_token, SqlStatement(stmt, params)));

  EXPECT_NE(query_partition, QueryPartition());

  QueryPartition copy = query_partition;
  EXPECT_EQ(copy, query_partition);

  QueryPartition assign;
  assign = copy;
  EXPECT_EQ(assign, copy);

  QueryPartition moved = std::move(copy);
  EXPECT_EQ(moved, assign);
}

TEST(QueryPartitionTest, SerializeDeserialize) {
  QueryPartitionTester expected_partition(internal::MakeQueryPartition(
      "foo", "session", "token",
      SqlStatement("select * from foo where name = @name",
                   {{"name", Value("Bob")}})));
  StatusOr<QueryPartition> partition = DeserializeQueryPartition(
      *(SerializeQueryPartition(expected_partition.Partition())));

  ASSERT_TRUE(partition.ok());
  QueryPartitionTester actual_partition = QueryPartitionTester(*partition);
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

TEST(QueryPartitionTest, FailedDeserialize) {
  std::string bad_serialized_proto("ThisIsNotTheProtoYouAreLookingFor");
  StatusOr<QueryPartition> partition =
      DeserializeQueryPartition(bad_serialized_proto);
  EXPECT_FALSE(partition.ok());
}

TEST(QueryPartitionTest, MakeSqlParams) {
  QueryPartitionTester expected_partition(internal::MakeQueryPartition(
      "foo", "session", "token",
      SqlStatement("select * from foo where name = @name",
                   {{"name", Value("Bob")}})));

  Connection::SqlParams params =
      internal::MakeSqlParams(expected_partition.Partition());

  EXPECT_EQ(params.statement,
            SqlStatement("select * from foo where name = @name",
                         {{"name", Value("Bob")}}));
  EXPECT_EQ(*params.partition_token, "token");
  EXPECT_THAT(params.transaction, HasSessionAndTransactionId("session", "foo"));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
