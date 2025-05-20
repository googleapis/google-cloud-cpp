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

#include "google/cloud/storage/internal/async/object_descriptor_connection_tracing.h"
#include "google/cloud/storage/async/object_descriptor.h"
#include "google/cloud/storage/async/reader.h"
#include "google/cloud/storage/async/reader_connection.h"
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

class AsyncObjectDescriptorConnectionTracing
    : public storage_experimental::ObjectDescriptorConnection {
 public:
  explicit AsyncObjectDescriptorConnectionTracing(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      std::shared_ptr<storage_experimental::ObjectDescriptorConnection> impl)
      : span_(std::move(span)), impl_(std::move(impl)) {}

  ~AsyncObjectDescriptorConnectionTracing() override {
    internal::EndSpan(*span_);
  }

  Options options() const override { return impl_->options(); }

  absl::optional<google::storage::v2::Object> metadata() const override {
    return impl_->metadata();
  }

  std::unique_ptr<storage_experimental::AsyncReaderConnection> Read(
      ReadParams p) override {
    internal::OTelScope scope(span_);
    auto result = impl_->Read(p);
    span_->AddEvent("gl-cpp.open.read",
                    {{sc::kThreadId, internal::CurrentThreadId()},
                     {"read-start", p.start},
                     {"read-length", p.length}});
    return result;
  }

  void MakeSubsequentStream() override {
    return impl_->MakeSubsequentStream();
  };

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  std::shared_ptr<storage_experimental::ObjectDescriptorConnection> impl_;
};

}  // namespace

std::shared_ptr<storage_experimental::ObjectDescriptorConnection>
MakeTracingObjectDescriptorConnection(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    std::shared_ptr<storage_experimental::ObjectDescriptorConnection> impl) {
  return std::make_unique<AsyncObjectDescriptorConnectionTracing>(
      std::move(span), std::move(impl));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
