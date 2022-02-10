// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/admin_client_params.h"
#include "google/cloud/grpc_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {

bool operator==(CompletionQueue const& a, CompletionQueue const& b) {
  using internal::GetCompletionQueueImpl;
  return GetCompletionQueueImpl(a) == GetCompletionQueueImpl(b);
}

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsNull;
using ::testing::NotNull;

TEST(AdminClientParams, WithSuppliedThreads) {
  CompletionQueue user_cq;
  auto cq_opts = Options{}.set<GrpcCompletionQueueOption>(user_cq);
  auto params = AdminClientParams(cq_opts);

  EXPECT_THAT(params.background_threads, IsNull());
  ASSERT_TRUE(params.options.has<GrpcCompletionQueueOption>());
  EXPECT_EQ(user_cq, params.cq);
  EXPECT_EQ(user_cq, params.options.get<GrpcCompletionQueueOption>());
}

TEST(AdminClientParams, WithoutSuppliedThreads) {
  auto params = AdminClientParams(Options{});

  EXPECT_THAT(params.background_threads, NotNull());
  ASSERT_TRUE(params.options.has<GrpcCompletionQueueOption>());
  EXPECT_EQ(params.background_threads->cq(),
            params.options.get<GrpcCompletionQueueOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
