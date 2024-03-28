// Copyright 2023 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/message_propagator.h"
#include "google/cloud/pubsub/internal/message_carrier.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/trace_propagator.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/scope.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::EqualsSpanContext;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::testing::_;
using ::testing::Contains;
using ::testing::Pair;
using ::testing::StartsWith;

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeTestSpan() {
  return internal::GetTracer(internal::CurrentOptions())
      ->StartSpan("test span");
}

TEST(MessagePropagatorTest, InjectTraceContext) {
  auto span_catcher = InstallSpanCatcher();
  opentelemetry::trace::Scope scope(MakeTestSpan());
  auto message = pubsub::MessageBuilder().Build();
  auto propagator =
      std::make_shared<opentelemetry::trace::propagation::HttpTraceContext>();

  InjectTraceContext(message, *propagator);

  EXPECT_THAT(message.attributes(),
              Contains(Pair(StartsWith("googclient_"), _)));
}

TEST(MessagePropagatorTest, ExtractTraceContext) {
  auto span_catcher = InstallSpanCatcher();
  auto test_span = MakeTestSpan();
  opentelemetry::trace::Scope scope(test_span);
  auto message = pubsub::MessageBuilder().Build();
  auto propagator =
      std::make_shared<opentelemetry::trace::propagation::HttpTraceContext>();
  InjectTraceContext(message, *propagator);

  auto context = ExtractTraceContext(message, *propagator);

  auto extracted_span = opentelemetry::trace::GetSpan(context);
  EXPECT_THAT(extracted_span->GetContext(),
              EqualsSpanContext(test_span->GetContext()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
