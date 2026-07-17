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
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/version.h"
#include "google/storage/v2/storage.pb.h"
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
      : impl_(std::move(impl)),
        cache_(std::make_shared<BucketMetadataCache>()) {}

  ~AsyncConnectionTracing() override = default;

  Options options() const override { return impl_->options(); }

  future<StatusOr<google::storage::v2::Bucket>> GetBucket(
      GetBucketParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::GetBucket");
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span), impl_->GetBucket(std::move(p)));
  }

  future<StatusOr<google::storage::v2::Object>> InsertObject(
      InsertObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::InsertObject");
    EnrichSpan(*span, p.options,
               p.request.write_object_spec().resource().bucket());
    internal::OTelScope scope(span);
    return impl_->InsertObject(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
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
      if (!reader) {
        return internal::EndSpan(*span, std::move(reader).status());
      }
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
          if (!w) {
            return internal::EndSpan(*span, std::move(w).status());
          }
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
          if (!w) {
            return internal::EndSpan(*span, std::move(w).status());
          }
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
          if (!w) {
            return internal::EndSpan(*span, std::move(w).status());
          }
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
          if (!w) {
            return internal::EndSpan(*span, std::move(w).status());
          }
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
    return impl_->ComposeObject(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  future<Status> DeleteObject(DeleteObjectParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::DeleteObject");
    EnrichSpan(*span, p.options, p.request.bucket());
    internal::OTelScope scope(span);
    return impl_->DeleteObject(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  std::shared_ptr<storage::AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) override {
    auto const enabled = internal::TracingEnabled(p.options);
    return MakeTracingAsyncRewriterConnection(
        impl_->RewriteObject(std::move(p)), enabled);
  }

  future<StatusOr<google::storage::v2::Bucket>> GetBucket(
      GetBucketParams p) override {
    auto span = internal::MakeSpan("storage::AsyncConnection::GetBucket");
    internal::OTelScope scope(span);
    auto const bucket_name = p.request.name();
    auto const options = p.options;
    return impl_->GetBucket(std::move(p))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span), this, bucket_name,
               options](future<StatusOr<google::storage::v2::Bucket>> f)
                  -> StatusOr<google::storage::v2::Bucket> {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          if (result.ok()) {
            EnrichSpan(*span, options, *result, bucket_name);
          } else {
            cache().MaybeInvalidate(result, bucket_name);
          }
          return internal::EndSpan(*span, std::move(result));
        });
  }

 private:
  BucketMetadataCache& cache() const { return *cache_; }

  void MaybeTriggerBackgroundFetch(Options const& options,
                                   std::string const& bucket_name) {
    if (!cache().StartFetch(bucket_name)) {
      return;
    }

    auto guard = ScopedFetch(cache_, bucket_name);
    google::storage::v2::GetBucketRequest request;
    auto const normalized_bucket_name =
        BucketMetadataCache::NormalizeBucketName(bucket_name);
    request.set_name("projects/_/buckets/" + normalized_bucket_name);
    GetBucketParams params{std::move(request), options};

    impl_->GetBucket(std::move(params))
        .then([cache = cache_, bucket_name, guard = std::move(guard)](
                  future<StatusOr<google::storage::v2::Bucket>> f) {
          auto metadata = f.get();
          if (metadata.ok()) {
            BucketCacheEntry entry = BucketCacheEntry::FromLocation(
                metadata->project() + "/buckets/" +
                    BucketMetadataCache::NormalizeBucketName(bucket_name),
                metadata->location(), metadata->location_type());
            cache->Put(bucket_name, std::move(entry));
          } else if (metadata.status().code() ==
                     StatusCode::kPermissionDenied) {
            BucketCacheEntry entry{
                "projects/_/buckets/" +
                    BucketMetadataCache::NormalizeBucketName(bucket_name),
                "global"};
            cache->Put(bucket_name, std::move(entry));
          }
        });
  }

  static void EnrichSpan(opentelemetry::trace::Span& span,
                         BucketCacheEntry const& entry) {
    span.SetAttribute("gcp.resource.destination.id", entry.id);
    span.SetAttribute("gcp.resource.destination.location", entry.location);
  }

  void EnrichSpan(opentelemetry::trace::Span& span, Options const& options,
                  google::storage::v2::Bucket const& bucket,
                  std::string const& bucket_name) {
    auto const enabled = options.get<
        google::cloud::storage_experimental::OTelSpanEnrichmentOption>();
    if (!enabled) return;
    auto entry = BucketCacheEntry::FromLocation(
        bucket.project() + "/buckets/" +
            BucketMetadataCache::NormalizeBucketName(bucket_name),
        bucket.location(), bucket.location_type());
    EnrichSpan(span, entry);
    cache().Put(bucket_name, std::move(entry));
  }

  void EnrichSpan(opentelemetry::trace::Span& span, Options const& options,
                  std::string const& bucket_name) {
    if (bucket_name.empty()) return;
    auto const enabled = options.get<
        google::cloud::storage_experimental::OTelSpanEnrichmentOption>();
    if (!enabled) return;
    auto entry = cache().Get(bucket_name);
    if (entry.has_value()) {
      EnrichSpan(span, *entry);
    } else {
      MaybeTriggerBackgroundFetch(options, bucket_name);
    }
  }

  std::shared_ptr<storage::AsyncConnection> impl_;
  std::shared_ptr<BucketMetadataCache> cache_;
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
