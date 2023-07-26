// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "rpc_retry_policy.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ms = std::chrono::milliseconds;

Options TestOptions() {
  return Options{}
      // Stay on this machine. Don't make a connection to the actual service.
      .set<EndpointOption>("localhost:1")
      .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
      // Disable channel refreshing, which is not under test.
      .set<MaxConnectionRefreshOption>(ms(0))
      // Create the minimum number of stubs.
      .set<GrpcNumChannelsOption>(1)
      // Disable retries
      .set<DataRetryPolicyOption>(
          std::make_shared<DataLimitedErrorCountRetryPolicy>(0));
}

TEST(MakeDataConnection, DefaultsOptions) {
  auto conn = MakeDataConnection(
      TestOptions().set<AppProfileIdOption>("user-supplied"));
  auto options = conn->options();
  EXPECT_TRUE(options.has<DataBackoffPolicyOption>())
      << "Options are not defaulted in MakeDataConnection()";
  EXPECT_EQ(options.get<AppProfileIdOption>(), "user-supplied")
      << "User supplied Options are overridden in MakeDataConnection()";
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::Contains;
using ::testing::IsEmpty;

TEST(MakeDataConnection, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto conn = MakeDataConnection(EnableTracing(TestOptions()));
  google::cloud::internal::OptionsSpan o(conn->options());
  (void)conn->Apply("table-name", SingleRowMutation("row"));

  EXPECT_THAT(span_catcher->GetSpans(),
              Contains(SpanNamed("bigtable::Table::Apply")));
}

TEST(MakeDataConnection, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto conn = MakeDataConnection(DisableTracing(TestOptions()));
  google::cloud::internal::OptionsSpan o(conn->options());
  (void)conn->Apply("table-name", SingleRowMutation("row"));

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
