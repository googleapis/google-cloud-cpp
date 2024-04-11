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

#include "google/cloud/storage/internal/async/connection_tracing.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/async/reader_connection_tracing.h"
#include "google/cloud/storage/internal/async/rewriter_connection_tracing.h"
#include "google/cloud/storage/internal/async/writer_connection_tracing.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

class AsyncConnectionTracing : public storage_experimental::AsyncConnection {
 public:
  explicit AsyncConnectionTracing(
      std::shared_ptr<storage_experimental::AsyncConnection> impl)
      : impl_(std::move(impl)) {}

  Options options() const override { return impl_->options(); }

  future<StatusOr<google::storage::v2::Object>> InsertObject(
      InsertObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::InsertObject");
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             impl_->InsertObject(std::move(p)));
  }

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
  ReadObject(ReadObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ReadObject");
    internal::OTelScope scope(span);
    auto wrap = [oc = opentelemetry::context::RuntimeContext::GetCurrent(),
                 span = std::move(span)](auto f)
        -> StatusOr<
            std::unique_ptr<storage_experimental::AsyncReaderConnection>> {
      auto reader = f.get();
      internal::DetachOTelContext(oc);
      if (!reader) return internal::EndSpan(*span, std::move(reader).status());
      return MakeTracingReaderConnection(std::move(span), *std::move(reader));
    };
    return impl_->ReadObject(std::move(p)).then(std::move(wrap));
  }

  future<StatusOr<storage_experimental::ReadPayload>> ReadObjectRange(
      ReadObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ReadObjectRange");
    internal::OTelScope scope(span);
    return impl_->ReadObjectRange(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartUnbufferedUpload(UploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::StartUnbufferedUpload");
    internal::OTelScope scope(span);
    return impl_->StartUnbufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<
                      storage_experimental::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartBufferedUpload(UploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::StartBufferedUpload");
    internal::OTelScope scope(span);
    return impl_->StartBufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<
                      storage_experimental::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeUnbufferedUpload(ResumeUploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::ResumeUnbufferedUpload");
    internal::OTelScope scope(span);
    return impl_->ResumeUnbufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<
                      storage_experimental::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeBufferedUpload(ResumeUploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::ResumeBufferedUpload");
    internal::OTelScope scope(span);
    return impl_->ResumeBufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<
                      storage_experimental::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      ComposeObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ComposeObject");
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             impl_->ComposeObject(std::move(p)));
  }

  future<Status> DeleteObject(DeleteObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::DeleteObject");
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             impl_->DeleteObject(std::move(p)));
  }

  std::shared_ptr<storage_experimental::AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) override {
    auto const enabled = internal::TracingEnabled(p.options);
    return MakeTracingAsyncRewriterConnection(
        impl_->RewriteObject(std::move(p)), enabled);
  }

 private:
  std::shared_ptr<storage_experimental::AsyncConnection> impl_;
};

}  // namespace

std::shared_ptr<storage_experimental::AsyncConnection>
MakeTracingAsyncConnection(
    std::shared_ptr<storage_experimental::AsyncConnection> impl) {
  if (!internal::TracingEnabled(impl->options())) return impl;
  return std::make_unique<AsyncConnectionTracing>(std::move(impl));
}

#else

std::shared_ptr<storage_experimental::AsyncConnection>
MakeTracingAsyncConnection(
    std::shared_ptr<storage_experimental::AsyncConnection> impl) {
  return impl;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
