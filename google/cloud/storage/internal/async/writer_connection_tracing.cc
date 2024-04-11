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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/storage/internal/async/writer_connection_tracing.h"
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

class AsyncWriterConnectionTracing
    : public storage_experimental::AsyncWriterConnection {
 public:
  explicit AsyncWriterConnectionTracing(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      std::unique_ptr<storage_experimental::AsyncWriterConnection> impl)
      : span_(std::move(span)), impl_(std::move(impl)) {}

  void Cancel() override {
    auto scope = opentelemetry::trace::Scope(span_);
    span_->AddEvent("gl-cpp.cancel",
                    {
                        {sc::kThreadId, internal::CurrentThreadId()},
                    });
    return impl_->Cancel();
  }

  std::string UploadId() const override {
    // No tracing, this is a local call without any significant work.
    return impl_->UploadId();
  }

  absl::variant<std::int64_t, google::storage::v2::Object> PersistedState()
      const override {
    // No tracing, this is a local call without any significant work.
    return impl_->PersistedState();
  }

  future<Status> Write(storage_experimental::WritePayload p) override {
    internal::OTelScope scope(span_);
    auto size = static_cast<std::uint64_t>(p.size());
    return impl_->Write(std::move(p))
        .then([count = ++sent_count_, span = span_, size](auto f) {
          span->AddEvent("gl-cpp.write",
                         {
                             {sc::kMessageType, "SENT"},
                             {sc::kMessageId, count},
                             {sc::kThreadId, internal::CurrentThreadId()},
                             {"gl-cpp.size", size},
                         });
          auto status = f.get();
          if (!status.ok()) return internal::EndSpan(*span, std::move(status));
          return status;
        });
  }

  future<StatusOr<google::storage::v2::Object>> Finalize(
      storage_experimental::WritePayload p) override {
    internal::OTelScope scope(span_);
    auto size = static_cast<std::uint64_t>(p.size());
    return impl_->Finalize(std::move(p))
        .then([count = ++sent_count_, span = span_, size](auto f) {
          span->AddEvent("gl-cpp.finalize",
                         {
                             {sc::kMessageType, "SENT"},
                             {sc::kMessageId, count},
                             {sc::kThreadId, internal::CurrentThreadId()},
                             {"gl-cpp.size", size},
                         });
          return internal::EndSpan(*span, f.get());
        });
  }

  future<Status> Flush(storage_experimental::WritePayload p) override {
    internal::OTelScope scope(span_);
    auto size = static_cast<std::uint64_t>(p.size());
    return impl_->Flush(std::move(p))
        .then([count = ++sent_count_, span = span_, size](auto f) {
          span->AddEvent("gl-cpp.flush",
                         {
                             {sc::kMessageType, "SENT"},
                             {sc::kMessageId, count},
                             {sc::kThreadId, internal::CurrentThreadId()},
                             {"gl-cpp.size", size},
                         });
          auto status = f.get();
          if (!status.ok()) return internal::EndSpan(*span, std::move(status));
          return status;
        });
  }

  future<StatusOr<std::int64_t>> Query() override {
    internal::OTelScope scope(span_);
    return impl_->Query().then([count = ++recv_count_, span = span_](auto f) {
      span->AddEvent("gl-cpp.query",
                     {
                         {sc::kMessageType, "RECEIVE"},
                         {sc::kMessageId, count},
                         {sc::kThreadId, internal::CurrentThreadId()},
                     });
      auto response = f.get();
      if (!response) return internal::EndSpan(*span, std::move(response));
      return response;
    });
  }

  RpcMetadata GetRequestMetadata() override {
    return impl_->GetRequestMetadata();
  }

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  std::unique_ptr<storage_experimental::AsyncWriterConnection> impl_;
  std::int64_t sent_count_ = 0;
  std::int64_t recv_count_ = 0;
};

}  // namespace

std::unique_ptr<storage_experimental::AsyncWriterConnection>
MakeTracingWriterConnection(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    std::unique_ptr<storage_experimental::AsyncWriterConnection> impl) {
  return std::make_unique<AsyncWriterConnectionTracing>(std::move(span),
                                                        std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
