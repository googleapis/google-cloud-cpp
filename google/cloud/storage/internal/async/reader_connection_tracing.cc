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

#include "google/cloud/storage/internal/async/reader_connection_tracing.h"

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/opentelemetry.h"
#include <opentelemetry/trace/semantic_conventions.h>
#include <cstdint>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = ::opentelemetry::trace::SemanticConventions;

class AsyncReaderConnectionTracing
    : public storage_experimental::AsyncReaderConnection {
 public:
  explicit AsyncReaderConnectionTracing(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      std::unique_ptr<storage_experimental::AsyncReaderConnection> impl)
      : span_(std::move(span)), impl_(std::move(impl)) {}

  void Cancel() override {
    auto scope = opentelemetry::trace::Scope(span_);
    span_->AddEvent("gl-cpp.cancel",
                    {
                        {sc::kThreadId, internal::CurrentThreadId()},
                    });
    return impl_->Cancel();
  }

  future<ReadResponse> Read() override {
    internal::OTelScope scope(span_);
    return impl_->Read()
        .then([count = ++count_, span = span_](auto f) -> ReadResponse {
          auto r = f.get();
          if (absl::holds_alternative<Status>(r)) {
            span->AddEvent(
                "gl-cpp.read",
                {
                    {/*sc::kRpcMessageType=*/"rpc.message.type", "RECEIVED"},
                    {/*sc::kRpcMessageId=*/"rpc.message.id", count},
                    {sc::kThreadId, internal::CurrentThreadId()},
                });
            return internal::EndSpan(*span, absl::get<Status>(std::move(r)));
          }
          auto const& payload = absl::get<storage_experimental::ReadPayload>(r);
          span->AddEvent(
              "gl-cpp.read",
              {
                  {/*sc::kRpcMessageType=*/"rpc.message.type", "RECEIVED"},
                  {/*sc::kRpcMessageId=*/"rpc.message.id", count},
                  {sc::kThreadId, internal::CurrentThreadId()},
                  {"message.starting_offset", payload.offset()},
              });
          return r;
        })
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent()](
                  auto f) {
          auto t = f.get();
          internal::DetachOTelContext(oc);
          return t;
        });
  }

  RpcMetadata GetRequestMetadata() override {
    return impl_->GetRequestMetadata();
  }

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  std::unique_ptr<storage_experimental::AsyncReaderConnection> impl_;
  std::int64_t count_ = 0;
};

}  // namespace

std::unique_ptr<storage_experimental::AsyncReaderConnection>
MakeTracingReaderConnection(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    std::unique_ptr<storage_experimental::AsyncReaderConnection> impl) {
  return std::make_unique<AsyncReaderConnectionTracing>(std::move(span),
                                                        std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
