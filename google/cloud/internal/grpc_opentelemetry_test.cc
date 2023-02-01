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

#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Pair;

TEST(OpenTelemetry, MakeSpanGrpc) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar");
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.cloud.foo.v1.Foo/GetBar"),
          SpanHasAttributes(
              SpanAttribute<std::string>(sc::kRpcSystem,
                                         sc::RpcSystemValues::kGrpc),
              SpanAttribute<std::string>(sc::kRpcService,
                                         "google.cloud.foo.v1.Foo"),
              SpanAttribute<std::string>(sc::kRpcMethod, "GetBar"),
              SpanAttribute<std::string>(sc::kNetTransport,
                                         sc::NetTransportValues::kIpTcp),
              SpanAttribute<std::string>("grpc.version", grpc::Version())))));
}

TEST(OpenTelemetry, InjectTraceContextGrpc) {
  auto mock_propagator = testing_util::InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject)
      .WillOnce([](auto& carrier, auto const&) {
        carrier.Set("x-test-key", "test-value");
      });

  grpc::ClientContext context;
  InjectTraceContext(context, Options{});

  testing_util::ValidateMetadataFixture fixture;
  auto md = fixture.GetMetadata(context);
  EXPECT_THAT(md, ElementsAre(Pair("x-test-key", "test-value")));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
