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

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <cstdint>
#include <regex>
#include <string>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;

MATCHER_P(MatchesStdRegex, pattern, "") {
  if (std::regex_match(arg, std::regex(pattern))) {
    return true;
  }
  *result_listener << "which does not match regex \"" << pattern << "\"";
  return false;
}

class ObservabilityIntegrationTest
    : public spanner_testing::DatabaseIntegrationTest {};

/// @test Verify OpenTelemetry tracing records Spanner Request ID attributes.
TEST_F(ObservabilityIntegrationTest, OpenTelemetryRequestIdTracing) {
  auto span_catcher = InstallSpanCatcher();

  auto options =
      Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_GZIP);
  auto session_mode =
      internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_TESTING_SESSION_MODE");
  if (session_mode.has_value() && *session_mode == "multiplexed") {
    options.set<spanner::EnableMultiplexedSessionOption>({});
  }
  options = EnableTracing(std::move(options));

  auto client = Client(MakeConnection(GetDatabase(), std::move(options)));

  auto rows = client.ExecuteQuery(SqlStatement("SELECT 1"));
  using RowType = std::tuple<std::int64_t>;
  auto row = GetSingularRow(StreamOf<RowType>(rows));
  ASSERT_STATUS_OK(row);
  EXPECT_THAT(std::get<0>(*row), Eq(1));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Not(IsEmpty()));

  auto request_id_matcher =
      MatchesStdRegex(R"(^1\.[0-9a-f]{16}\.[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$)");

  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanNamed("google.spanner.v1.Spanner/ExecuteStreamingSql"),
                     SpanHasAttributes(OTelAttribute<std::string>(
                         "gcp.spanner.request_id", request_id_matcher)))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
