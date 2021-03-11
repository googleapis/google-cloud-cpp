// Copyright 2020 Google LLC
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

#include "google/cloud/internal/common_options.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "generator/integration_tests/golden/internal/golden_thing_admin_stub_factory.gcpcxx.pb.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::testing::HasSubstr;

TEST(ResolveGoldenThingAdminOptions, DefaultEndpoint) {
  internal::Options options;
  auto resolved_options = ResolveGoldenThingAdminOptions(options);
  EXPECT_EQ("test.googleapis.com",
            resolved_options.get<internal::EndpointOption>());
}

TEST(ResolveGoldenThingAdminOptions, EnvVarEndpoint) {
  internal::SetEnv("GOOGLE_CLOUD_CPP_GOLDEN_THING_ADMIN_ENDPOINT",
                   "foo.googleapis.com");
  internal::Options options;
  auto resolved_options = ResolveGoldenThingAdminOptions(options);
  EXPECT_EQ("foo.googleapis.com",
            resolved_options.get<internal::EndpointOption>());
}

TEST(ResolveGoldenThingAdminOptions, OptionEndpoint) {
  internal::SetEnv("GOOGLE_CLOUD_CPP_GOLDEN_KITCHEN_SINK_ENDPOINT",
                   "foo.googleapis.com");
  internal::Options options;
  options.set<internal::EndpointOption>("bar.googleapis.com");
  auto resolved_options = ResolveGoldenThingAdminOptions(options);
  EXPECT_EQ("bar.googleapis.com",
            resolved_options.get<internal::EndpointOption>());
}

class GoldenStubFactoryTest : public ::testing::Test {
 protected:
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
};

TEST_F(GoldenStubFactoryTest, DefaultStubWithoutLogging) {
  auto default_stub = CreateDefaultGoldenThingAdminStub({});
  auto const log_lines = log_.ExtractLines();
  EXPECT_EQ(log_lines.size(), 0);
}

TEST_F(GoldenStubFactoryTest, DefaultStubWithLogging) {
  internal::Options options;
  options.set<internal::TracingComponentsOption>({"rpc"});
  auto default_stub = CreateDefaultGoldenThingAdminStub(options);
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Enabled logging for gRPC calls")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
