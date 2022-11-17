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

#include "generator/integration_tests/golden/internal/golden_thing_admin_rest_stub_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(GoldenThingAdminRestStubFactoryTest, DefaultStubWithoutLogging) {
  testing_util::ScopedLog log;
  auto default_stub = CreateDefaultGoldenThingAdminRestStub({});
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, IsEmpty());
}

TEST(GoldenThingAdminRestStubFactoryTest, DefaultStubWithLogging) {
  testing_util::ScopedLog log;
  Options options;
  options.set<TracingComponentsOption>({"http"});
  auto default_stub = CreateDefaultGoldenThingAdminRestStub(options);
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Enabled logging for HTTP calls")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
