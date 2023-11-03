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

#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class QueryPartitionTester {
 public:
  QueryPartitionTester() = default;
  explicit QueryPartitionTester(spanner::QueryPartition partition)
      : partition_(std::move(partition)) {}
  spanner::SqlStatement const& Statement() const {
    return partition_.sql_statement();
  }
  bool DataBoost() const { return partition_.data_boost(); }
  std::string const& PartitionToken() const {
    return partition_.partition_token();
  }
  std::string const& SessionId() const { return partition_.session_id(); }
  std::string const& TransactionId() const {
    return partition_.transaction_id();
  }
  bool RouteToLeader() const { return partition_.route_to_leader(); }
  std::string const& TransactionTag() const {
    return partition_.transaction_tag();
  }
  spanner::QueryPartition Partition() const { return partition_; }

 private:
  spanner::QueryPartition partition_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::spanner_internal::QueryPartitionTester;
using ::google::cloud::spanner_testing::HasSessionAndTransaction;
using ::google::cloud::testing_util::IsOk;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::Property;
using ::testing::VariantWith;

TEST(QueryPartitionTest, MakeQueryPartition) {
  std::string stmt("SELECT * FROM foo WHERE name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  bool data_boost = true;
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("txn-id");
  bool route_to_leader = false;
  std::string transaction_tag("tag");

  QueryPartitionTester actual_partition(spanner_internal::MakeQueryPartition(
      transaction_id, route_to_leader, transaction_tag, session_id,
      partition_token, data_boost, SqlStatement(stmt, params)));
  EXPECT_EQ(stmt, actual_partition.Statement().sql());
  EXPECT_EQ(params, actual_partition.Statement().params());
  EXPECT_EQ(partition_token, actual_partition.PartitionToken());
  EXPECT_EQ(transaction_id, actual_partition.TransactionId());
  EXPECT_EQ(route_to_leader, actual_partition.RouteToLeader());
  EXPECT_EQ(transaction_tag, actual_partition.TransactionTag());
  EXPECT_EQ(session_id, actual_partition.SessionId());
  EXPECT_EQ(data_boost, actual_partition.DataBoost());
}

TEST(QueryPartitionTest, RegularSemantics) {
  std::string stmt("SELECT * FROM foo WHERE name = @name");
  SqlStatement::ParamType params = {{"name", Value("Bob")}};
  bool data_boost = true;
  std::string partition_token("token");
  std::string session_id("session");
  std::string transaction_id("txn-id");
  bool route_to_leader = true;
  std::string transaction_tag("tag");

  QueryPartition query_partition(spanner_internal::MakeQueryPartition(
      transaction_id, route_to_leader, transaction_tag, session_id,
      partition_token, data_boost, SqlStatement(stmt, params)));

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
  QueryPartitionTester expected_partition(spanner_internal::MakeQueryPartition(
      "txn-id", true, "tag", "session", "token", false,
      SqlStatement("SELECT * FROM foo WHERE name = @name",
                   {{"name", Value("Bob")}})));
  StatusOr<QueryPartition> partition = DeserializeQueryPartition(
      *(SerializeQueryPartition(expected_partition.Partition())));

  ASSERT_STATUS_OK(partition);
  QueryPartitionTester actual_partition = QueryPartitionTester(*partition);
  EXPECT_EQ(expected_partition.PartitionToken(),
            actual_partition.PartitionToken());
  EXPECT_EQ(expected_partition.TransactionId(),
            actual_partition.TransactionId());
  EXPECT_EQ(expected_partition.RouteToLeader(),
            actual_partition.RouteToLeader());
  EXPECT_EQ(expected_partition.TransactionTag(),
            actual_partition.TransactionTag());
  EXPECT_EQ(expected_partition.SessionId(), actual_partition.SessionId());
  EXPECT_EQ(expected_partition.Statement().sql(),
            actual_partition.Statement().sql());
  EXPECT_EQ(expected_partition.Statement().params(),
            actual_partition.Statement().params());
  EXPECT_EQ(expected_partition.DataBoost(), actual_partition.DataBoost());
}

TEST(QueryPartitionTest, FailedDeserialize) {
  std::string bad_serialized_proto("ThisIsNotTheProtoYouAreLookingFor");
  StatusOr<QueryPartition> partition =
      DeserializeQueryPartition(bad_serialized_proto);
  EXPECT_THAT(partition, Not(IsOk()));
}

TEST(QueryPartitionTest, MakeSqlParams) {
  QueryPartitionTester expected_partition(spanner_internal::MakeQueryPartition(
      "txn-id", true, "tag", "session", "token", true,
      SqlStatement("SELECT * FROM foo WHERE name = @name",
                   {{"name", Value("Bob")}})));

  Connection::SqlParams params = spanner_internal::MakeSqlParams(
      expected_partition.Partition(),
      QueryOptions{}.set_request_tag("request_tag"),
      ExcludeReplicas({ReplicaSelection(ReplicaType::kReadWrite)}));

  EXPECT_EQ(params.statement,
            SqlStatement("SELECT * FROM foo WHERE name = @name",
                         {{"name", Value("Bob")}}));
  EXPECT_EQ(*params.partition_token, "token");
  EXPECT_TRUE(params.partition_data_boost);
  EXPECT_THAT(params.transaction,
              HasSessionAndTransaction("session", "txn-id", true, "tag"));
  EXPECT_EQ(*params.query_options.request_tag(), "request_tag");
  EXPECT_THAT(params.directed_read_option,
              VariantWith<ExcludeReplicas>(Property(
                  &ExcludeReplicas::replica_selections,
                  ElementsAre(ReplicaSelection(ReplicaType::kReadWrite)))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
