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

#include "google/cloud/spanner/internal/operation_context.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::StrEq;

class OperationContextTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

  testing_util::ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(OperationContextTest, PreCallSetsRequestIdHeader) {
  auto static_prefix =
      std::make_shared<std::string const>("1.0123456789abcdef.1.");
  OperationContext op_context(static_prefix, 42, "ExecuteSql");

  EXPECT_THAT(op_context.request_index(), Eq(42ULL));
  EXPECT_THAT(op_context.attempt_index(), Eq(0U));
  EXPECT_THAT(op_context.channel_id(), Eq(0U));
  EXPECT_THAT(op_context.rpc_name(), StrEq("ExecuteSql"));
  EXPECT_THAT(op_context.RequestId(), Eq(std::nullopt));

  op_context.BindChannel(3);
  EXPECT_THAT(op_context.channel_id(), Eq(3U));

  grpc::ClientContext context1;
  op_context.PreCall(context1);

  EXPECT_THAT(op_context.attempt_index(), Eq(1U));
  EXPECT_THAT(op_context.RequestId(),
              Optional(Eq("1.0123456789abcdef.1.3.42.1")));

  auto metadata = GetMetadata(context1);
  EXPECT_THAT(metadata, Contains(Pair("x-goog-spanner-request-id",
                                      "1.0123456789abcdef.1.3.42.1")));

  // Next attempt increments attempt_index to 2
  grpc::ClientContext context2;
  op_context.PreCall(context2);

  EXPECT_THAT(op_context.attempt_index(), Eq(2U));
  EXPECT_THAT(op_context.RequestId(),
              Optional(Eq("1.0123456789abcdef.1.3.42.2")));

  metadata = GetMetadata(context2);
  EXPECT_THAT(metadata, Contains(Pair("x-goog-spanner-request-id",
                                      "1.0123456789abcdef.1.3.42.2")));
}

TEST_F(OperationContextTest, MoveConstructAndAssign) {
  auto static_prefix =
      std::make_shared<std::string const>("1.0123456789abcdef.1.");
  OperationContext op_context(static_prefix, 10, "Commit");
  op_context.BindChannel(2);

  grpc::ClientContext context1;
  op_context.PreCall(context1);
  EXPECT_THAT(op_context.attempt_index(), Eq(1U));

  // Move construct
  OperationContext moved_context(std::move(op_context));
  EXPECT_THAT(moved_context.request_index(), Eq(10ULL));
  EXPECT_THAT(moved_context.channel_id(), Eq(2U));
  EXPECT_THAT(moved_context.attempt_index(), Eq(1U));
  EXPECT_THAT(moved_context.rpc_name(), StrEq("Commit"));
  EXPECT_THAT(moved_context.RequestId(),
              Optional(Eq("1.0123456789abcdef.1.2.10.1")));

  // Move assign
  OperationContext target_context(static_prefix, 99, "Rollback");
  target_context = std::move(moved_context);
  EXPECT_THAT(target_context.request_index(), Eq(10ULL));
  EXPECT_THAT(target_context.channel_id(), Eq(2U));
  EXPECT_THAT(target_context.attempt_index(), Eq(1U));
  EXPECT_THAT(target_context.rpc_name(), StrEq("Commit"));
  EXPECT_THAT(target_context.RequestId(),
              Optional(Eq("1.0123456789abcdef.1.2.10.1")));
}

TEST_F(OperationContextTest, PostCallAndOnDoneHooks) {
  auto static_prefix =
      std::make_shared<std::string const>("1.0123456789abcdef.1.");
  OperationContext op_context(static_prefix, 1, "ExecuteSql");

  grpc::ClientContext context;
  op_context.PreCall(context);
  op_context.PostCall(context, Status{});
  op_context.OnDone(Status{});
  EXPECT_THAT(op_context.attempt_index(), Eq(1U));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
