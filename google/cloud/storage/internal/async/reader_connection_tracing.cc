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
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/async/reader_connection_telemetry.h"
#include "google/cloud/internal/opentelemetry.h"
#include <opentelemetry/semconv/incubating/thread_attributes.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = ::opentelemetry::semconv;

class AsyncReaderConnectionTracing : public storage::AsyncReaderConnection {
 public:
  explicit AsyncReaderConnectionTracing(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      std::unique_ptr<storage::AsyncReaderConnection> impl,
      std::string bucket_name)
      : span_(std::move(span)),
        impl_(std::move(impl)),
        bucket_name_(
            std::make_shared<std::string const>(std::move(bucket_name))) {}

  void Cancel() override {
    auto scope = opentelemetry::trace::Scope(span_);
    span_->AddEvent("gl-cpp.cancel",
                    {
                        {sc::thread::kThreadId, internal::CurrentThreadId()},
                    });
    return impl_->Cancel();
  }

  future<ReadResponse> Read() override {
    internal::OTelScope scope(span_);
    return impl_->Read()
        .then([count = ++count_, span = span_, bucket_name = bucket_name_,
               metrics = metrics_](auto f) -> ReadResponse {
          auto r = f.get();
          if (auto const* status = absl::get_if<Status>(&r)) {
            span->AddEvent(
                "gl-cpp.read",
                {
                    {/*sc::kRpcMessageType=*/"rpc.message.type", "RECEIVED"},
                    {/*sc::kRpcMessageId=*/"rpc.message.id", count},
                    {sc::thread::kThreadId, internal::CurrentThreadId()},
                });
            return internal::EndSpan(*span, *status);
          }
          auto const& payload = absl::get<storage::ReadPayload>(r);
          span->AddEvent(
              "gl-cpp.read",
              {
                  {/*sc::kRpcMessageType=*/"rpc.message.type", "RECEIVED"},
                  {/*sc::kRpcMessageId=*/"rpc.message.id", count},
                  {sc::thread::kThreadId, internal::CurrentThreadId()},
                  {"message.starting_offset", payload.offset()},
              });
          metrics.RecordRead(payload, std::chrono::steady_clock::now(),
                             *bucket_name, span);
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
  std::unique_ptr<storage::AsyncReaderConnection> impl_;
  std::int64_t count_ = 0;
  std::shared_ptr<std::string const> bucket_name_;
  ReaderConnectionTelemetry metrics_;
};

}  // namespace

std::unique_ptr<storage::AsyncReaderConnection> MakeTracingReaderConnection(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    std::unique_ptr<storage::AsyncReaderConnection> impl,
    std::string bucket_name) {
  return std::make_unique<AsyncReaderConnectionTracing>(
      std::move(span), std::move(impl), std::move(bucket_name));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
