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

#include "google/cloud/storage/internal/async/rewriter_connection_tracing.h"
#include "google/cloud/storage/async/rewriter_connection.h"
#include "google/cloud/internal/opentelemetry.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

using google::cloud::internal::EndSpan;

class AsyncRewriterTracingConnection
    : public storage_experimental::AsyncRewriterConnection {
 public:
  AsyncRewriterTracingConnection(
      std::shared_ptr<storage_experimental::AsyncRewriterConnection> impl,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span)
      : impl_(std::move(impl)), span_(std::move(span)) {}
  ~AsyncRewriterTracingConnection() override { EndSpan(*span_, Status{}); }

  future<StatusOr<google::storage::v2::RewriteResponse>> Iterate() override {
    internal::OTelScope scope(span_);
    return impl_->Iterate().then(
        [span = span_,
         oc = opentelemetry::context::RuntimeContext::GetCurrent()](auto f) {
          auto r = f.get();
          internal::DetachOTelContext(oc);
          if (!r) {
            span->AddEvent("gl-cpp.storage.rewrite.iterate",
                           {{"gl-cpp.status_code",
                             static_cast<std::int32_t>(r.status().code())}});
            return r;
          }
          span->AddEvent("gl-cpp.storage.rewrite.iterate",
                         {{"gl-cpp.status_code",
                           static_cast<std::int32_t>(StatusCode::kOk)},
                          {"total_bytes_rewritten", r->total_bytes_rewritten()},
                          {"object_size", r->object_size()}});
          if (r->has_resource()) return EndSpan(*span, std::move(r));
          return r;
        });
  }

 private:
  std::shared_ptr<storage_experimental::AsyncRewriterConnection> impl_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
};

}  // namespace

std::shared_ptr<storage_experimental::AsyncRewriterConnection>
MakeTracingAsyncRewriterConnection(
    std::shared_ptr<storage_experimental::AsyncRewriterConnection> impl,
    bool enabled) {
  if (!enabled) return impl;
  auto span = internal::MakeSpan("storage::AsyncConnection::RewriteObject");
  return std::make_shared<AsyncRewriterTracingConnection>(std::move(impl),
                                                          std::move(span));
}

#else

std::shared_ptr<storage_experimental::AsyncRewriterConnection>
MakeTracingAsyncRewriterConnection(
    std::shared_ptr<storage_experimental::AsyncRewriterConnection> impl,
    bool /*enabled*/) {
  return impl;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
