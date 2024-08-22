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

#include "google/cloud/opentelemetry/trace_exporter.h"
#include "google/cloud/trace/v2/mocks/mock_trace_connection.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::ScopedLog;
using ::google::devtools::cloudtrace::v2::BatchWriteSpansRequest;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

std::vector<std::string> SpanNames(BatchWriteSpansRequest const& request) {
  auto const& spans = request.spans();
  std::vector<std::string> names;
  names.reserve(spans.size());
  std::transform(spans.begin(), spans.end(), std::back_inserter(names),
                 [](auto const& span) { return span.display_name().value(); });
  return names;
}

TEST(TraceExporter, Success) {
  auto mock = std::make_shared<trace_v2_mocks::MockTraceServiceConnection>();
  EXPECT_CALL(*mock, BatchWriteSpans)
      .WillOnce([](BatchWriteSpansRequest const& request) {
        EXPECT_EQ(request.name(), "projects/test-project");
        EXPECT_THAT(SpanNames(request), ElementsAre("span-1", "span-2"));
        return Status();
      });

  auto exporter = otel_internal::MakeTraceExporter(Project("test-project"),
                                                   std::move(mock));

  std::vector<std::unique_ptr<opentelemetry::sdk::trace::Recordable>> v;
  v.emplace_back(exporter->MakeRecordable());
  v.back()->SetName("span-1");
  v.emplace_back(exporter->MakeRecordable());
  v.back()->SetName("span-2");

  auto result = exporter->Export({v.data(), v.size()});
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);
}

TEST(TraceExporter, Failure) {
  auto mock = std::make_shared<trace_v2_mocks::MockTraceServiceConnection>();
  EXPECT_CALL(*mock, BatchWriteSpans)
      .WillOnce([](BatchWriteSpansRequest const& request) {
        EXPECT_EQ(request.name(), "projects/test-project");
        EXPECT_THAT(SpanNames(request), ElementsAre("span"));
        return internal::AbortedError("fail");
      });

  auto exporter = otel_internal::MakeTraceExporter(Project("test-project"),
                                                   std::move(mock));

  auto recordable = exporter->MakeRecordable();
  recordable->SetName("span");

  auto result = exporter->Export({&recordable, 1});
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

TEST(TraceExporter, LogsOnError) {
  // Disable library logging for a more conclusive test.
  ScopedEnvironment env{"GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt};

  ScopedLog log;

  auto mock = std::make_shared<trace_v2_mocks::MockTraceServiceConnection>();
  EXPECT_CALL(*mock, BatchWriteSpans)
      .WillOnce(Return(internal::UnavailableError("try again later")));

  auto exporter = otel_internal::MakeTraceExporter(Project("test-project"),
                                                   std::move(mock));

  auto recordable = exporter->MakeRecordable();
  recordable->SetName("span");

  auto result = exporter->Export({&recordable, 1});
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);

  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("Cloud Trace Export"), HasSubstr("1 span(s)"),
                     HasSubstr("UNAVAILABLE"), HasSubstr("try again later"))));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsEmpty;

TEST(TraceExporter, NoInfiniteExportLoop14611) {
  auto span_catcher = InstallSpanCatcher();

  ScopedEnvironment env{"GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", "ON"};

  auto project = Project("test-project");
  auto options = Options{}
                     .set<EndpointOption>("localhost:1")
                     .set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  auto exporter = MakeTraceExporter(project, options);

  // Simulate an export which should not create any additional spans.
  auto recordable = exporter->MakeRecordable();
  recordable->SetName("span");
  (void)exporter->Export({&recordable, 1});

  // Verify that no spans were created.
  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
