// Copyright 2022 Google LLC
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

#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/connection_tracing.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/grpc_options.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeBackgroundThreadsFactory;

AsyncClient::AsyncClient(Options options) {
  options = storage_internal::DefaultOptionsAsync(std::move(options));
  background_ = MakeBackgroundThreadsFactory(options)();
  connection_ = storage_internal::MakeTracingAsyncConnection(
      storage_internal::MakeAsyncConnection(background_->cq(),
                                            std::move(options)));
}

AsyncClient::AsyncClient(std::shared_ptr<AsyncConnection> connection)
    : connection_(std::move(connection)) {}

future<StatusOr<google::storage::v2::Object>> AsyncClient::InsertObject(
    google::storage::v2::WriteObjectRequest request, WritePayload contents,
    Options opts) {
  return connection_->InsertObject(
      {std::move(request), std::move(contents),
       internal::MergeOptions(std::move(opts), connection_->options())});
}

future<StatusOr<std::pair<AsyncReader, AsyncToken>>> AsyncClient::ReadObject(
    BucketName const& bucket_name, std::string object_name, Options opts) {
  auto request = google::storage::v2::ReadObjectRequest{};
  request.set_bucket(bucket_name.FullName());
  request.set_object(std::move(object_name));
  return ReadObject(std::move(request), std::move(opts));
}

future<StatusOr<std::pair<AsyncReader, AsyncToken>>> AsyncClient::ReadObject(
    google::storage::v2::ReadObjectRequest request, Options opts) {
  return connection_
      ->ReadObject(
          {std::move(request),
           internal::MergeOptions(std::move(opts), connection_->options())})
      .then([](auto f) -> StatusOr<std::pair<AsyncReader, AsyncToken>> {
        auto impl = f.get();
        if (!impl) return std::move(impl).status();
        auto t = storage_internal::MakeAsyncToken(impl->get());
        return std::make_pair(AsyncReader(*std::move(impl)), std::move(t));
      });
}

future<StatusOr<ReadPayload>> AsyncClient::ReadObjectRange(
    BucketName const& bucket_name, std::string object_name, std::int64_t offset,
    std::int64_t limit, Options opts) {
  auto request = google::storage::v2::ReadObjectRequest{};
  request.set_bucket(bucket_name.FullName());
  request.set_object(std::move(object_name));

  return ReadObjectRange(std::move(request), offset, limit, std::move(opts));
}

future<StatusOr<ReadPayload>> AsyncClient::ReadObjectRange(
    google::storage::v2::ReadObjectRequest request, std::int64_t offset,
    std::int64_t limit, Options opts) {
  request.set_read_offset(offset);
  request.set_read_limit(limit);

  return connection_->ReadObjectRange(
      {std::move(request),
       internal::MergeOptions(std::move(opts), connection_->options())});
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::StartBufferedUpload(BucketName const& bucket_name,
                                 std::string object_name, Options opts) {
  auto request = google::storage::v2::StartResumableWriteRequest{};
  auto& resource = *request.mutable_write_object_spec()->mutable_resource();
  resource.set_bucket(bucket_name.FullName());
  resource.set_name(std::move(object_name));
  return StartBufferedUpload(std::move(request), std::move(opts));
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::StartBufferedUpload(
    google::storage::v2::StartResumableWriteRequest request, Options opts) {
  return connection_
      ->StartBufferedUpload(
          {std::move(request),
           internal::MergeOptions(std::move(opts), connection_->options())})
      .then([](auto f) -> StatusOr<std::pair<AsyncWriter, AsyncToken>> {
        auto w = f.get();
        if (!w) return std::move(w).status();
        auto t = absl::holds_alternative<google::storage::v2::Object>(
                     (*w)->PersistedState())
                     ? AsyncToken()
                     : storage_internal::MakeAsyncToken(w->get());
        return std::make_pair(AsyncWriter(*std::move(w)), std::move(t));
      });
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::ResumeBufferedUpload(std::string upload_id, Options opts) {
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id(std::move(upload_id));
  return ResumeBufferedUpload(std::move(request), std::move(opts));
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::ResumeBufferedUpload(
    google::storage::v2::QueryWriteStatusRequest request, Options opts) {
  return connection_
      ->ResumeBufferedUpload(
          {std::move(request),
           internal::MergeOptions(std::move(opts), connection_->options())})
      .then([](auto f) -> StatusOr<std::pair<AsyncWriter, AsyncToken>> {
        auto w = f.get();
        if (!w) return std::move(w).status();
        auto t = absl::holds_alternative<google::storage::v2::Object>(
                     (*w)->PersistedState())
                     ? AsyncToken()
                     : storage_internal::MakeAsyncToken(w->get());
        return std::make_pair(AsyncWriter(*std::move(w)), std::move(t));
      });
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::StartUnbufferedUpload(BucketName const& bucket_name,
                                   std::string object_name, Options opts) {
  auto request = google::storage::v2::StartResumableWriteRequest{};
  auto& resource = *request.mutable_write_object_spec()->mutable_resource();
  resource.set_bucket(bucket_name.FullName());
  resource.set_name(std::move(object_name));
  return StartUnbufferedUpload(std::move(request), std::move(opts));
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::StartUnbufferedUpload(
    google::storage::v2::StartResumableWriteRequest request, Options opts) {
  return connection_
      ->StartUnbufferedUpload(
          {std::move(request),
           internal::MergeOptions(std::move(opts), connection_->options())})
      .then([](auto f) -> StatusOr<std::pair<AsyncWriter, AsyncToken>> {
        auto w = f.get();
        if (!w) return std::move(w).status();
        auto t = absl::holds_alternative<google::storage::v2::Object>(
                     (*w)->PersistedState())
                     ? AsyncToken()
                     : storage_internal::MakeAsyncToken(w->get());
        return std::make_pair(AsyncWriter(*std::move(w)), std::move(t));
      });
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::ResumeUnbufferedUpload(std::string upload_id, Options opts) {
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id(std::move(upload_id));
  return ResumeUnbufferedUpload(std::move(request), std::move(opts));
}

future<StatusOr<std::pair<AsyncWriter, AsyncToken>>>
AsyncClient::ResumeUnbufferedUpload(
    google::storage::v2::QueryWriteStatusRequest request, Options opts) {
  return connection_
      ->ResumeUnbufferedUpload(
          {std::move(request),
           internal::MergeOptions(std::move(opts), connection_->options())})
      .then([](auto f) -> StatusOr<std::pair<AsyncWriter, AsyncToken>> {
        auto w = f.get();
        if (!w) return std::move(w).status();
        auto t = absl::holds_alternative<google::storage::v2::Object>(
                     (*w)->PersistedState())
                     ? AsyncToken()
                     : storage_internal::MakeAsyncToken(w->get());
        return std::make_pair(AsyncWriter(*std::move(w)), std::move(t));
      });
}

future<StatusOr<google::storage::v2::Object>> AsyncClient::ComposeObject(
    BucketName const& bucket_name, std::string destination_object_name,
    std::vector<google::storage::v2::ComposeObjectRequest::SourceObject>
        source_objects,
    Options opts) {
  google::storage::v2::ComposeObjectRequest request;
  request.mutable_destination()->set_bucket(bucket_name.FullName());
  request.mutable_destination()->set_name(std::move(destination_object_name));
  for (auto& source : source_objects) {
    request.mutable_source_objects()->Add(std::move(source));
  }
  return ComposeObject(std::move(request), std::move(opts));
}

future<StatusOr<google::storage::v2::Object>> AsyncClient::ComposeObject(
    google::storage::v2::ComposeObjectRequest request, Options opts) {
  return connection_->ComposeObject(
      {std::move(request),
       internal::MergeOptions(std::move(opts), connection_->options())});
}

future<Status> AsyncClient::DeleteObject(BucketName const& bucket_name,
                                         std::string object_name,
                                         Options opts) {
  google::storage::v2::DeleteObjectRequest request;
  request.set_bucket(bucket_name.FullName());
  request.set_object(object_name);
  return DeleteObject(std::move(request), std::move(opts));
}

future<Status> AsyncClient::DeleteObject(BucketName const& bucket_name,
                                         std::string object_name,
                                         std::int64_t generation,
                                         Options opts) {
  google::storage::v2::DeleteObjectRequest request;
  request.set_bucket(bucket_name.FullName());
  request.set_object(object_name);
  request.set_generation(generation);
  return DeleteObject(std::move(request), std::move(opts));
}

future<Status> AsyncClient::DeleteObject(
    google::storage::v2::DeleteObjectRequest request, Options opts) {
  return connection_->DeleteObject(
      {std::move(request),
       internal::MergeOptions(std::move(opts), connection_->options())});
}

std::pair<AsyncRewriter, AsyncToken> AsyncClient::StartRewrite(
    BucketName const& source_bucket, std::string source_object_name,
    BucketName const& destination_bucket, std::string destination_object_name,
    Options opts) {
  auto request = google::storage::v2::RewriteObjectRequest{};
  request.set_destination_name(std::move(destination_object_name));
  request.set_destination_bucket(destination_bucket.FullName());
  request.set_source_object(std::move(source_object_name));
  request.set_source_bucket(source_bucket.FullName());
  return ResumeRewrite(std::move(request), std::move(opts));
}

std::pair<AsyncRewriter, AsyncToken> AsyncClient::StartRewrite(
    google::storage::v2::RewriteObjectRequest request, Options opts) {
  request.mutable_rewrite_token()->clear();
  return ResumeRewrite(std::move(request), std::move(opts));
}

std::pair<AsyncRewriter, AsyncToken> AsyncClient::ResumeRewrite(
    BucketName const& source_bucket, std::string source_object_name,
    BucketName const& destination_bucket, std::string destination_object_name,
    std::string rewrite_token, Options opts) {
  google::storage::v2::RewriteObjectRequest request;
  request.set_destination_name(std::move(destination_object_name));
  request.set_destination_bucket(destination_bucket.FullName());
  request.set_source_object(std::move(source_object_name));
  request.set_source_bucket(source_bucket.FullName());
  request.set_rewrite_token(std::move(rewrite_token));
  return ResumeRewrite(std::move(request), std::move(opts));
}

std::pair<AsyncRewriter, AsyncToken> AsyncClient::ResumeRewrite(
    google::storage::v2::RewriteObjectRequest request, Options opts) {
  auto c = connection_->RewriteObject(
      {std::move(request),
       internal::MergeOptions(std::move(opts), connection_->options())});
  auto token = storage_internal::MakeAsyncToken(c.get());
  return std::make_pair(AsyncRewriter(std::move(c)), std::move(token));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
