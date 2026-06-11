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
#include "google/cloud/storage/internal/async/object_descriptor_connection_tracing.h"
#include "google/cloud/storage/internal/async/reader_connection_tracing.h"
#include "google/cloud/storage/internal/async/rewriter_connection_tracing.h"
#include "google/cloud/storage/internal/async/writer_connection_tracing.h"
#include "google/cloud/storage/internal/bucket_metadata_cache.h"
#include "google/cloud/storage/internal/connection_factory.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/version.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {

namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

class AsyncConnectionTracing : public storage::AsyncConnection {
 public:
  explicit AsyncConnectionTracing(
      std::shared_ptr<storage::AsyncConnection> impl)
      : impl_(std::move(impl)), cache_(BucketMetadataCache::Singleton()) {}

  ~AsyncConnectionTracing() override {
    for (auto& f : bg_tasks_) {
      if (f.valid()) f.wait();
    }
  }

  Options options() const override { return impl_->options(); }

  future<StatusOr<google::storage::v2::Object>> InsertObject(
      InsertObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::InsertObject");
    EnrichSpan(*span, p.options,
               p.request.write_object_spec().resource().bucket());
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             impl_->InsertObject(std::move(p)));
  }

  future<StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>> Open(
      OpenParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::Open");
    EnrichSpan(*span, p.options, p.read_spec.bucket());
    internal::OTelScope scope(span);
    return impl_->Open(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<
                      std::shared_ptr<storage::ObjectDescriptorConnection>> {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          if (!result) {
            return internal::EndSpan(*span, std::move(result).status());
          }
          return MakeTracingObjectDescriptorConnection(std::move(span),
                                                       *std::move(result));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncReaderConnection>>> ReadObject(
      ReadObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ReadObject");
    EnrichSpan(*span, p.options, p.request.bucket());
    internal::OTelScope scope(span);
    auto wrap = [oc = opentelemetry::context::RuntimeContext::GetCurrent(),
                 span = std::move(span)](auto f)
        -> StatusOr<std::unique_ptr<storage::AsyncReaderConnection>> {
      auto reader = f.get();
      internal::DetachOTelContext(oc);
      if (!reader) return internal::EndSpan(*span, std::move(reader).status());
      return MakeTracingReaderConnection(std::move(span), *std::move(reader));
    };
    return impl_->ReadObject(std::move(p)).then(std::move(wrap));
  }

  future<StatusOr<storage::ReadPayload>> ReadObjectRange(
      ReadObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ReadObjectRange");
    EnrichSpan(*span, p.options, p.request.bucket());
    internal::OTelScope scope(span);
    return impl_->ReadObjectRange(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
  StartAppendableObjectUpload(AppendableUploadParams p) override {
    auto span = internal::MakeSpan(
        "storage::AsyncConnection::StartAppendableObjectUpload");
    EnrichSpan(*span, p.options,
               p.request.write_object_spec().resource().bucket());
    internal::OTelScope scope(span);
    return impl_->StartAppendableObjectUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
  ResumeAppendableObjectUpload(AppendableUploadParams p) override {
    auto span = internal::MakeSpan(
        "storage::AsyncConnection::ResumeAppendableObjectUpload");
    EnrichSpan(*span, p.options,
               p.request.write_object_spec().resource().bucket());
    internal::OTelScope scope(span);
    return impl_->ResumeAppendableObjectUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
  StartUnbufferedUpload(UploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::StartUnbufferedUpload");
    EnrichSpan(*span, p.options,
               p.request.write_object_spec().resource().bucket());
    internal::OTelScope scope(span);
    return impl_->StartUnbufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
  StartBufferedUpload(UploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::StartBufferedUpload");
    EnrichSpan(*span, p.options,
               p.request.write_object_spec().resource().bucket());
    internal::OTelScope scope(span);
    return impl_->StartBufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
  ResumeUnbufferedUpload(ResumeUploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::ResumeUnbufferedUpload");
    internal::OTelScope scope(span);
    return impl_->ResumeUnbufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
  ResumeBufferedUpload(ResumeUploadParams p) override {
    auto span =
        internal::MakeSpan("storage::AsyncConnection::ResumeBufferedUpload");
    internal::OTelScope scope(span);
    return impl_->ResumeBufferedUpload(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f)
                  -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto w = f.get();
          internal::DetachOTelContext(oc);
          if (!w) return internal::EndSpan(*span, std::move(w).status());
          return MakeTracingWriterConnection(span, *std::move(w));
        });
  }

  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      ComposeObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::ComposeObject");
    EnrichSpan(*span, p.options, p.request.destination().bucket());
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             impl_->ComposeObject(std::move(p)));
  }

  future<Status> DeleteObject(DeleteObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::DeleteObject");
    EnrichSpan(*span, p.options, p.request.bucket());
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             impl_->DeleteObject(std::move(p)));
  }

  std::shared_ptr<storage::AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) override {
    auto const enabled = internal::TracingEnabled(p.options);
    return MakeTracingAsyncRewriterConnection(
        impl_->RewriteObject(std::move(p)), enabled);
  }

 private:
  void CleanupCompletedTasks() {
    std::unique_lock<std::mutex> lk(mu_);
    bg_tasks_.erase(
        std::remove_if(bg_tasks_.begin(), bg_tasks_.end(),
                       [](std::future<void> const& f) {
                         return f.wait_for(std::chrono::seconds(0)) ==
                                std::future_status::ready;
                       }),
        bg_tasks_.end());
  }

  void MaybeTriggerBackgroundFetch(Options const& options,
                                   std::string const& bucket_name) {
    CleanupCompletedTasks();

    if (!cache_.StartFetch(bucket_name)) {
      return;
    }

    std::shared_ptr<storage::internal::StorageConnection> conn;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (!sync_conn_) {
        sync_conn_ = MakeStorageConnection(impl_->options());
      }
      conn = sync_conn_;
    }

    auto f =
        std::async(std::launch::async, [bucket_name, options, conn, this]() {
          google::cloud::internal::OptionsSpan span(options);
          auto const normalized_bucket_name =
              BucketMetadataCache::NormalizeBucketName(bucket_name);
          storage::internal::GetBucketMetadataRequest request(
              normalized_bucket_name);
          auto metadata = conn->GetBucketMetadata(request);
          auto entry = BucketCacheEntry::Create(bucket_name, metadata);
          if (entry) {
            cache_.Put(bucket_name, std::move(*entry));
          }
          cache_.EndFetch(bucket_name);
        });

    std::unique_lock<std::mutex> lk(mu_);
    bg_tasks_.push_back(std::move(f));
  }

  void EnrichSpan(opentelemetry::trace::Span& span, Options const& options,
                  std::string const& bucket_name) {
    if (bucket_name.empty()) return;
    auto entry = cache_.Get(bucket_name);
    if (entry.has_value()) {
      span.SetAttribute("gcp.resource.destination.id", entry->id);
      span.SetAttribute("gcp.resource.destination.location", entry->location);
    } else {
      MaybeTriggerBackgroundFetch(options, bucket_name);
    }
  }

  std::shared_ptr<storage::AsyncConnection> impl_;
  BucketMetadataCache& cache_;
  std::shared_ptr<storage::internal::StorageConnection> sync_conn_;
  std::vector<std::future<void>> bg_tasks_;
  std::mutex mu_;
};

}  // namespace

std::shared_ptr<storage::AsyncConnection> MakeTracingAsyncConnection(
    std::shared_ptr<storage::AsyncConnection> impl) {
  if (!internal::TracingEnabled(impl->options())) return impl;
  return std::make_unique<AsyncConnectionTracing>(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
