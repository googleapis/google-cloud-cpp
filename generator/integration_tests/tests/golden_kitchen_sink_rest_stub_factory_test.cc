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
#include "google/cloud/credentials.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain_options.h"
#include <generator/integration_tests/test.pb.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::GenerateIdTokenRequest;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;

TEST(GoldenKitchenSinkRestStubFactoryTest, DefaultStubWithoutLogging) {
  testing_util::ScopedLog log;
  Options options;
  auto default_stub = CreateDefaultGoldenKitchenSinkRestStub(options);
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, IsEmpty());
  ASSERT_TRUE(options.has<EndpointOption>());
  EXPECT_THAT(options.get<EndpointOption>(),
              Eq("goldenkitchensink.googleapis.com."));
}

TEST(GoldenKitchenSinkRestStubFactoryTest, DefaultStubWithLogging) {
  testing_util::ScopedLog log;
  Options options;
  options.set<TracingComponentsOption>({"rpc"});
  auto default_stub = CreateDefaultGoldenKitchenSinkRestStub(options);
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("Enabled logging for REST rpc calls")));
}

TEST(GoldenKitchenSinkRestStubFactoryTest,
     DefaultStubWithUniverseDomainOption) {
  Options options;
  options.set<EndpointOption>("localhost:1")
      .set<internal::UniverseDomainOption>("not empty")
      .set<UnifiedCredentialsOption>(MakeAccessTokenCredentials(
          "invalid-access-token",
          std::chrono::system_clock::now() + std::chrono::minutes(15)));
  auto default_stub = CreateDefaultGoldenKitchenSinkRestStub(options);
  ASSERT_TRUE(options.has<EndpointOption>());
  EXPECT_THAT(options.get<EndpointOption>(), Eq("localhost:1"));

  rest_internal::RestContext rest_context;
  auto response = default_stub->GenerateIdToken(rest_context, options,
                                                GenerateIdTokenRequest{});
  EXPECT_THAT(
      response,
      Not(AnyOf(IsOk(),
                StatusIs(StatusCode::kInvalidArgument,
                         HasSubstr("UniverseDomainOption cannot be empty")))));
}

TEST(GoldenKitchenSinkRestStubFactoryTest,
     DefaultStubWithEmptyUniverseDomainOption) {
  Options options;
  options.set<internal::UniverseDomainOption>("").set<UnifiedCredentialsOption>(
      MakeAccessTokenCredentials(
          "invalid-access-token",
          std::chrono::system_clock::now() + std::chrono::minutes(15)));
  auto default_stub = CreateDefaultGoldenKitchenSinkRestStub(options);
  EXPECT_FALSE(options.has<EndpointOption>());

  rest_internal::RestContext rest_context;
  auto response = default_stub->GenerateIdToken(rest_context, options,
                                                GenerateIdTokenRequest{});
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("UniverseDomainOption cannot be empty")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
