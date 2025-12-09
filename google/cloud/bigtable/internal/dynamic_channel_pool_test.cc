// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/dynamic_channel_pool.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;

TEST(DynamicChannelPoolTest, GetChannelRandomTwoLeastUsed) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();

  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  auto stub_factory_fn = [](int) -> std::shared_ptr<BigtableStub> {
    return std::make_shared<MockBigtableStub>();
  };

  DynamicChannelPool<BigtableStub>::SizingPolicy sizing_policy;

  std::vector<std::shared_ptr<BigtableStub>> channels(10);
  int id = 0;
  std::generate(channels.begin(), channels.end(),
                [&]() { return stub_factory_fn(id++); });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      CompletionQueue(fake_cq_impl), channels, refresh_state, stub_factory_fn,
      sizing_policy);

  auto selected_stub = pool->GetChannelRandomTwoLeastUsed();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
