// Copyright 2024 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/async/object_descriptor_connection_tracing.h"
#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;
using ::google::cloud::storage_experimental::ObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::Return;

TEST(ObjectDescriptorConnectionTracing, Read) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](ObjectDescriptorConnection::ReadParams p) {
        EXPECT_EQ(p.start, 100);
        EXPECT_EQ(p.length, 200);
        return std::make_unique<MockAsyncReaderConnection>();
      });
  auto actual = MakeTracingObjectDescriptorConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  auto f1 = actual->Read(ObjectDescriptorConnection::ReadParams{100, 200});

  actual.reset();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("test-span-name"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanHasEvents(AllOf(
                      EventNamed("gl-cpp.open.read"),
                      SpanEventAttributesAre(
                          OTelAttribute<std::int64_t>("read-length", 200),
                          OTelAttribute<std::int64_t>("read-start", 100),
                          OTelAttribute<std::string>(sc::kThreadId, _)))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
