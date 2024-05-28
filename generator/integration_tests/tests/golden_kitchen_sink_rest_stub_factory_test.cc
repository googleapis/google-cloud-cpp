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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_rest_stub_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <generator/integration_tests/test.pb.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(GoldenKitchenSinkRestStubFactoryTest, DefaultStubWithoutLogging) {
  testing_util::ScopedLog log;
  auto default_stub = CreateDefaultGoldenKitchenSinkRestStub({});
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, IsEmpty());
}

TEST(GoldenKitchenSinkRestStubFactoryTest, DefaultStubWithLogging) {
  testing_util::ScopedLog log;
  Options options;
  options.set<LoggingComponentsOption>({"rpc"});
  auto default_stub = CreateDefaultGoldenKitchenSinkRestStub(options);
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("Enabled logging for REST rpc calls")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
