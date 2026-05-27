// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/tracing_connection.h"
#include "google/cloud/storage/internal/tracing_object_read_source.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/rest_pure_background_threads_impl.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TracingConnection::TracingConnection(std::shared_ptr<StorageConnection> impl,
                                     AsyncRunner runner)
    : impl_(std::move(impl)),
      cache_(std::make_shared<BucketMetadataCache>()),
      runner_(std::move(runner)) {}

TracingConnection::~TracingConnection() = default;

TracingConnection::AsyncRunner const& TracingConnection::runner() {
  absl::call_once(once_flag_, [this] {
    if (!runner_) {
      auto threads =
          std::make_shared<google::cloud::rest_internal::
                               AutomaticallyCreatedRestPureBackgroundThreads>(
              1U);
      runner_ = [threads](std::function<void()> f) {
        threads->cq().RunAsync(std::move(f));
      };
    }
  });
  return runner_;
}

BucketMetadataCache& TracingConnection::cache() const { return *cache_; }

TracingConnection::~TracingConnection() {
  for (auto& f : bg_tasks_) {
    if (f.valid()) f.wait();
  }
}

Options TracingConnection::options() const { return impl_->options(); }

void TracingConnection::CleanupCompletedTasks() {
  std::lock_guard<std::mutex> lock(mu_);
  bg_tasks_.erase(std::remove_if(bg_tasks_.begin(), bg_tasks_.end(),
                                 [](std::future<void> const& f) {
                                   return f.wait_for(std::chrono::seconds(0)) ==
                                          std::future_status::ready;
                                 }),
                  bg_tasks_.end());
}

void TracingConnection::MaybeTriggerBackgroundFetch(
    std::string const& bucket_name) {
  CleanupCompletedTasks();

  std::lock_guard<std::mutex> lock(mu_);
  if (in_flight_fetch_.find(bucket_name) != in_flight_fetch_.end()) {
    return;
  }

  in_flight_fetch_.insert(bucket_name);

  auto f = std::async(std::launch::async, [this, bucket_name]() {
    storage::internal::GetBucketMetadataRequest request(bucket_name);
    auto result = impl_->GetBucketMetadata(request);

    BucketCacheEntry entry;
    if (result.ok()) {
      entry.id = "projects/" + std::to_string(result->project_number()) +
                 "/buckets/" + result->name();
      entry.location = result->location();
      if (result->location_type() == "multi-region" ||
          result->location_type() == "dual-region") {
        entry.location = "global";
      }
      cache_.Put(bucket_name, std::move(entry));
    } else if (result.status().code() == StatusCode::kPermissionDenied) {
      entry.id = "projects/_/buckets/" + bucket_name;
      entry.location = "global";
      cache_.Put(bucket_name, std::move(entry));
    }

    std::lock_guard<std::mutex> lock(mu_);
    in_flight_fetch_.erase(bucket_name);
  });

  bg_tasks_.push_back(std::move(f));
}

void TracingConnection::EnrichSpan(opentelemetry::trace::Span& span,
                                   std::string const& bucket_name) {
  if (bucket_name.empty()) return;
  auto entry = cache_.Get(bucket_name);
  if (entry.has_value()) {
    span.SetAttribute("gcp.resource.destination.id", entry->id);
    span.SetAttribute("gcp.resource.destination.location", entry->location);
  } else {
    MaybeTriggerBackgroundFetch(bucket_name);
  }
}

void TracingConnection::EnrichSpan(opentelemetry::trace::Span& span,
                                   BucketCacheEntry const& entry) {
  span.SetAttribute("gcp.resource.destination.id", entry.id);
  span.SetAttribute("gcp.resource.destination.location", entry.location);
}

void TracingConnection::MaybeTriggerBackgroundFetch(
    std::string const& bucket_name) {
  if (!cache().StartFetch(bucket_name)) {
    return;
  }

  auto guard = ScopedFetch(cache_, bucket_name);
  auto current_options = google::cloud::internal::SaveCurrentOptions();
  runner()([impl = impl_, cache = cache_, bucket_name, current_options,
            guard]() {
    google::cloud::internal::OptionsSpan span(current_options);
    storage::internal::GetBucketMetadataRequest request(bucket_name);
    auto result = impl->GetBucketMetadata(request);

    if (result.ok()) {
      cache->Put(bucket_name, BucketCacheEntry::FromMetadata(*result));
    } else if (result.status().code() == StatusCode::kPermissionDenied) {
      cache->Put(bucket_name, {"projects/_/buckets/" + bucket_name, "global"});
    }
  });
}

void TracingConnection::EnrichSpan(opentelemetry::trace::Span& span,
                                   std::string const& bucket_name) {
  if (bucket_name.empty()) return;
  auto const enabled =
      options().get<storage_experimental::OTelSpanEnrichmentOption>();
  if (!enabled) return;
  auto entry = cache().Get(bucket_name);
  if (entry.has_value()) {
    EnrichSpan(span, *entry);
  } else {
    MaybeTriggerBackgroundFetch(bucket_name);
  }
}

void TracingConnection::EnrichSpan(
    opentelemetry::trace::Span& span,
    storage::BucketMetadata const& metadata) const {
  auto const enabled =
      options().get<storage_experimental::OTelSpanEnrichmentOption>();
  if (!enabled) return;
  auto entry = BucketCacheEntry::FromMetadata(metadata);
  EnrichSpan(span, entry);
  cache().Put(metadata.name(), std::move(entry));
}

StatusOr<storage::internal::ListBucketsResponse> TracingConnection::ListBuckets(
    storage::internal::ListBucketsRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListBuckets");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListBuckets(request));
}

StatusOr<storage::BucketMetadata> TracingConnection::CreateBucket(
    storage::internal::CreateBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateBucket");
  auto scope = opentelemetry::trace::Scope(span);
  auto result = impl_->CreateBucket(request);
  if (result.ok()) EnrichSpan(*span, *result);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketMetadata> TracingConnection::GetBucketMetadata(
    storage::internal::GetBucketMetadataRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetBucketMetadata");
  auto scope = opentelemetry::trace::Scope(span);
  auto result = impl_->GetBucketMetadata(request);
  if (result.ok()) {
    EnrichSpan(*span, *result);
  } else {
    MaybeInvalidate(result, request.bucket_name());
  }
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteBucket(
    storage::internal::DeleteBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteBucket");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->DeleteBucket(request);
  if (result.ok() || result.status().code() == StatusCode::kNotFound) {
    cache().Invalidate(request.bucket_name());
  }
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketMetadata> TracingConnection::UpdateBucket(
    storage::internal::UpdateBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateBucket");
  auto scope = opentelemetry::trace::Scope(span);
  auto result = impl_->UpdateBucket(request);
  if (result.ok()) {
    EnrichSpan(*span, *result);
  } else {
    MaybeInvalidate(result, request.metadata().name());
  }
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketMetadata> TracingConnection::PatchBucket(
    storage::internal::PatchBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchBucket");
  auto scope = opentelemetry::trace::Scope(span);
  auto result = impl_->PatchBucket(request);
  if (result.ok()) {
    EnrichSpan(*span, *result);
  } else {
    MaybeInvalidate(result, request.bucket());
  }
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::NativeIamPolicy> TracingConnection::GetNativeBucketIamPolicy(
    storage::internal::GetBucketIamPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetNativeBucketIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->GetNativeBucketIamPolicy(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::NativeIamPolicy> TracingConnection::SetNativeBucketIamPolicy(
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::SetNativeBucketIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->SetNativeBucketIamPolicy(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
TracingConnection::TestBucketIamPermissions(
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::TestBucketIamPermissions");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->TestBucketIamPermissions(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketMetadata> TracingConnection::LockBucketRetentionPolicy(
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::LockBucketRetentionPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  auto result = impl_->LockBucketRetentionPolicy(request);
  if (result.ok()) EnrichSpan(*span, *result);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::InsertObjectMedia(
    storage::internal::InsertObjectMediaRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::InsertObjectMedia");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->InsertObjectMedia(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::CopyObject(
    storage::internal::CopyObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CopyObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.destination_bucket());
  auto result = impl_->CopyObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::GetObjectMetadata(
    storage::internal::GetObjectMetadataRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetObjectMetadata");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->GetObjectMetadata(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
TracingConnection::ReadObject(
    storage::internal::ReadObjectRangeRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ReadObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto reader = impl_->ReadObject(request);
  if (!reader) {
    return internal::EndSpan(*span, std::move(reader));
  }
  return std::unique_ptr<storage::internal::ObjectReadSource>(
      std::make_unique<TracingObjectReadSource>(std::move(span),
                                                *std::move(reader)));
}

StatusOr<storage::internal::ListObjectsResponse> TracingConnection::ListObjects(
    storage::internal::ListObjectsRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListObjects");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->ListObjects(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteObject(
    storage::internal::DeleteObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->DeleteObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::UpdateObject(
    storage::internal::UpdateObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->UpdateObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::MoveObject(
    storage::internal::MoveObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::MoveObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->MoveObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::PatchObject(
    storage::internal::PatchObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->PatchObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::ComposeObject(
    storage::internal::ComposeObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ComposeObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->ComposeObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::RewriteObjectResponse>
TracingConnection::RewriteObject(
    storage::internal::RewriteObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::RewriteObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.destination_bucket());
  auto result = impl_->RewriteObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectMetadata> TracingConnection::RestoreObject(
    storage::internal::RestoreObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::RestoreObject");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->RestoreObject(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::CreateResumableUploadResponse>
TracingConnection::CreateResumableUpload(
    storage::internal::ResumableUploadRequest const& request) {
  // TODO(#11394) - add a wrapper for WriteObject().
  auto span =
      internal::MakeSpan("storage::Client::WriteObject/CreateResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->CreateResumableUpload(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::QueryResumableUploadResponse>
TracingConnection::QueryResumableUpload(
    storage::internal::QueryResumableUploadRequest const& request) {
  // TODO(#11394) - add a wrapper for WriteObject().
  auto span =
      internal::MakeSpan("storage::Client::WriteObject/QueryResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->QueryResumableUpload(request));
}

StatusOr<storage::internal::EmptyResponse>
TracingConnection::DeleteResumableUpload(
    storage::internal::DeleteResumableUploadRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteResumableUpload(request));
}

StatusOr<storage::internal::QueryResumableUploadResponse>
TracingConnection::UploadChunk(
    storage::internal::UploadChunkRequest const& request) {
  // TODO(#11394) - add a wrapper for WriteObject().
  auto span = internal::MakeSpan("storage::Client::WriteObject/UploadChunk");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UploadChunk(request));
}

StatusOr<std::unique_ptr<std::string>> TracingConnection::UploadFileSimple(
    std::string const& file_name, std::size_t file_size,
    storage::internal::InsertObjectMediaRequest& request) {
  auto span =
      internal::MakeSpan("storage::Client::UploadFile/UploadFileSimple");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->UploadFileSimple(file_name, file_size, request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<std::unique_ptr<std::istream>> TracingConnection::UploadFileResumable(
    std::string const& file_name,
    storage::internal::ResumableUploadRequest& request) {
  auto span =
      internal::MakeSpan("storage::Client::UploadFile/UploadFileResumable");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->UploadFileResumable(file_name, request);
  return internal::EndSpan(*span, std::move(result));
}

Status TracingConnection::DownloadStreamToFile(
    storage::ObjectReadStream&& stream, std::string const& file_name,
    storage::internal::ReadObjectRangeRequest const& request) {
  auto span = internal::MakeSpan(
      "storage::Client::DownloadToFile/DownloadStreamToFile");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result =
      impl_->DownloadStreamToFile(std::move(stream), file_name, request);
  return internal::EndSpan(*span, result);
}

StatusOr<storage::ObjectMetadata> TracingConnection::ExecuteParallelUploadFile(
    std::vector<std::thread> threads,
    std::vector<storage::internal::ParallelUploadFileShard> shards,
    bool ignore_cleanup_failures) {
  auto span = internal::MakeSpan(
      "storage::ParallelUploadFile/ExecuteParallelUploadFile");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ExecuteParallelUploadFile(
                                      std::move(threads), std::move(shards),
                                      ignore_cleanup_failures));
}

StatusOr<storage::internal::ListBucketAclResponse>
TracingConnection::ListBucketAcl(
    storage::internal::ListBucketAclRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->ListBucketAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketAccessControl> TracingConnection::CreateBucketAcl(
    storage::internal::CreateBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->CreateBucketAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteBucketAcl(
    storage::internal::DeleteBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->DeleteBucketAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketAccessControl> TracingConnection::GetBucketAcl(
    storage::internal::GetBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->GetBucketAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketAccessControl> TracingConnection::UpdateBucketAcl(
    storage::internal::UpdateBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->UpdateBucketAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::BucketAccessControl> TracingConnection::PatchBucketAcl(
    storage::internal::PatchBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->PatchBucketAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::ListObjectAclResponse>
TracingConnection::ListObjectAcl(
    storage::internal::ListObjectAclRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->ListObjectAcl(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::CreateObjectAcl(
    storage::internal::CreateObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->CreateObjectAcl(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteObjectAcl(
    storage::internal::DeleteObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->DeleteObjectAcl(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::GetObjectAcl(
    storage::internal::GetObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->GetObjectAcl(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::UpdateObjectAcl(
    storage::internal::UpdateObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->UpdateObjectAcl(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::PatchObjectAcl(
    storage::internal::PatchObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->PatchObjectAcl(request);
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
TracingConnection::ListDefaultObjectAcl(
    storage::internal::ListDefaultObjectAclRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->ListDefaultObjectAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl>
TracingConnection::CreateDefaultObjectAcl(
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->CreateDefaultObjectAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::EmptyResponse>
TracingConnection::DeleteDefaultObjectAcl(
    storage::internal::DeleteDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->DeleteDefaultObjectAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::GetDefaultObjectAcl(
    storage::internal::GetDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->GetDefaultObjectAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl>
TracingConnection::UpdateDefaultObjectAcl(
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->UpdateDefaultObjectAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::PatchDefaultObjectAcl(
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->PatchDefaultObjectAcl(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::ServiceAccount> TracingConnection::GetServiceAccount(
    storage::internal::GetProjectServiceAccountRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetServiceAccount");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetServiceAccount(request));
}

StatusOr<storage::internal::ListHmacKeysResponse>
TracingConnection::ListHmacKeys(
    storage::internal::ListHmacKeysRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListHmacKeys");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListHmacKeys(request));
}

StatusOr<storage::internal::CreateHmacKeyResponse>
TracingConnection::CreateHmacKey(
    storage::internal::CreateHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateHmacKey(request));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteHmacKey(
    storage::internal::DeleteHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteHmacKey(request));
}

StatusOr<storage::HmacKeyMetadata> TracingConnection::GetHmacKey(
    storage::internal::GetHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetHmacKey(request));
}

StatusOr<storage::HmacKeyMetadata> TracingConnection::UpdateHmacKey(
    storage::internal::UpdateHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateHmacKey(request));
}

StatusOr<storage::internal::SignBlobResponse> TracingConnection::SignBlob(
    storage::internal::SignBlobRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::SignBlob");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->SignBlob(request));
}

StatusOr<storage::internal::ListNotificationsResponse>
TracingConnection::ListNotifications(
    storage::internal::ListNotificationsRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListNotifications");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->ListNotifications(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::NotificationMetadata> TracingConnection::CreateNotification(
    storage::internal::CreateNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateNotification");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->CreateNotification(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::NotificationMetadata> TracingConnection::GetNotification(
    storage::internal::GetNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetNotification");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->GetNotification(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

StatusOr<storage::internal::EmptyResponse>
TracingConnection::DeleteNotification(
    storage::internal::DeleteNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteNotification");
  auto scope = opentelemetry::trace::Scope(span);
  EnrichSpan(*span, request.bucket_name());
  auto result = impl_->DeleteNotification(request);
  MaybeInvalidate(result, request.bucket_name());
  return internal::EndSpan(*span, std::move(result));
}

std::vector<std::string> TracingConnection::InspectStackStructure() const {
  auto stack = impl_->InspectStackStructure();
  stack.emplace_back("TracingConnection");
  return stack;
}

std::shared_ptr<storage::internal::StorageConnection> MakeTracingClient(
    std::shared_ptr<storage::internal::StorageConnection> impl,
    TracingConnection::AsyncRunner runner) {
  return std::make_shared<TracingConnection>(std::move(impl),
                                             std::move(runner));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
