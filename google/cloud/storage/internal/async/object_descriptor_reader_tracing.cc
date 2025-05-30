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

#include "google/cloud/storage/internal/async/object_descriptor_reader_tracing.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/async/object_descriptor_reader.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/version.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

namespace sc = ::opentelemetry::trace::SemanticConventions;

class ObjectDescriptorReaderTracing : public ObjectDescriptorReader {
 public:
  explicit ObjectDescriptorReaderTracing(std::shared_ptr<ReadRange> impl)
      : ObjectDescriptorReader(std::move(impl)) {}

  ~ObjectDescriptorReaderTracing() override = default;

  future<ObjectDescriptorReader::ReadResponse> Read() override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ReadObjectRange");
    internal::OTelScope scope(span);
    return ObjectDescriptorReader::Read().then(
        [span = std::move(span),
         oc = opentelemetry::context::RuntimeContext::GetCurrent()](
            auto f) -> ReadResponse {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          if (!absl::holds_alternative<Status>(result)) {
            auto const& payload =
                absl::get<storage_experimental::ReadPayload>(result);

            span->AddEvent(
                "gl-cpp.read-range",
                {{/*sc::kRpcMessageType=*/"rpc.message.type", "RECEIVED"},
                 {sc::kThreadId, internal::CurrentThreadId()},
                 {"message.size", static_cast<std::uint32_t>(payload.size())}});
          } else {
            span->AddEvent(
                "gl-cpp.read-range",
                {{/*sc::kRpcMessageType=*/"rpc.message.type", "RECEIVED"},
                 {sc::kThreadId, internal::CurrentThreadId()}});
            return internal::EndSpan(*span,
                                     absl::get<Status>(std::move(result)));
          }
          return result;
        });
  }
};

}  // namespace

std::unique_ptr<storage_experimental::AsyncReaderConnection>
MakeTracingObjectDescriptorReader(std::shared_ptr<ReadRange> impl) {
  return std::make_unique<ObjectDescriptorReaderTracing>(std::move(impl));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::unique_ptr<storage_experimental::AsyncReaderConnection>
MakeTracingObjectDescriptorReader(std::shared_ptr<ReadRange> impl) {
  return std::make_unique<ObjectDescriptorReader>(std::move(impl));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
