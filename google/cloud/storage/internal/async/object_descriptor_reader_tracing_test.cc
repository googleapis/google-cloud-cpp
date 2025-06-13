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

#include "google/cloud/storage/internal/async/object_descriptor_reader_tracing.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanNamed;
using ::google::protobuf::TextFormat;
using ::testing::_;

namespace sc = ::opentelemetry::trace::SemanticConventions;

TEST(ObjectDescriptorReaderTracing, Read) {
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_shared<ReadRange>(10000, 30);
  auto reader = MakeTracingObjectDescriptorReader(impl);

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "0123456789" }
    read_range { read_offset: 10000 read_length: 10 read_id: 7 }
    range_end: false
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));
  impl->OnRead(std::move(data));

  auto actual = reader->Read().get();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::ReadRange"),
                       SpanHasEvents(AllOf(
                           EventNamed("gl-cpp.read-range"),
                           SpanEventAttributesAre(
                               OTelAttribute<std::uint32_t>("message.size", 10),
                               OTelAttribute<std::string>(sc::kThreadId, _),
                               OTelAttribute<std::string>("rpc.message.type",
                                                          "RECEIVED")))))));
}

TEST(ObjectDescriptorReaderTracing, ReadError) {
  auto span_catcher = InstallSpanCatcher();
  auto impl = std::make_shared<ReadRange>(10000, 30);
  auto reader = MakeTracingObjectDescriptorReader(impl);

  impl->OnFinish(PermanentError());

  auto actual = reader->Read().get();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::ReadRange"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", "NOT_FOUND")),
          SpanHasEvents(AllOf(EventNamed("gl-cpp.read-range"),
                              SpanEventAttributesAre(
                                  OTelAttribute<std::string>(sc::kThreadId, _),
                                  OTelAttribute<std::string>("rpc.message.type",
                                                             "RECEIVED")))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
