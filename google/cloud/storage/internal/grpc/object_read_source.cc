// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/grpc/object_read_source.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/type_traits.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <limits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

GrpcObjectReadSource::GrpcObjectReadSource(TimerSource timer_source,
                                           std::unique_ptr<StreamingRpc> stream)
    : timer_source_(std::move(timer_source)), stream_(std::move(stream)) {}

StatusOr<storage::internal::HttpResponse> GrpcObjectReadSource::Close() {
  if (stream_) stream_ = nullptr;
  if (!status_.ok()) return status_;
  return storage::internal::HttpResponse{
      storage::internal::HttpStatusCode::kOk, {}, {}};
}

/// Read more data from the download, returning any HTTP headers and error
/// codes.
StatusOr<storage::internal::ReadSourceResult> GrpcObjectReadSource::Read(
    char* buf, std::size_t n) {
  using google::storage::v2::ReadObjectResponse;

  storage::internal::ReadSourceResult result;
  result.response.status_code = storage::internal::HttpStatusCode::kContinue;
  result.bytes_received = buffer_.FillBuffer(buf, n);

  while (result.bytes_received < n && stream_) {
    auto watchdog = timer_source_().then([this](auto f) {
      if (!f.get()) return false;  // timer cancelled, no action needed
      stream_->Cancel();
      return true;
    });
    google::storage::v2::ReadObjectResponse response;
    auto status = stream_->Read(&response);
    watchdog.cancel();
    if (watchdog.get()) {
      status_ = google::cloud::internal::DeadlineExceededError(
          "Deadline exceeded waiting for data in ReadObject", GCP_ERROR_INFO());
      // The stream is already cancelled, but we need to wait for its status.
      while (!status.has_value()) status = stream_->Read(&response);
      return status_;
    }
    if (status.has_value()) {
      status_ = *std::move(status);
      auto metadata = stream_->GetRequestMetadata();
      result.response.headers.insert(metadata.headers.begin(),
                                     metadata.headers.end());
      result.response.headers.insert(metadata.trailers.begin(),
                                     metadata.trailers.end());
      stream_.reset();
      if (!status_.ok()) return status_;
      return result;
    }
    HandleResponse(result, buf, n, std::move(response));
  }

  return result;
}

void GrpcObjectReadSource::HandleResponse(
    storage::internal::ReadSourceResult& result, char* buf, std::size_t n,
    google::storage::v2::ReadObjectResponse response) {
  // The google.storage.v1.Storage documentation says this field can be
  // empty.
  if (response.has_checksummed_data()) {
    auto const offset = result.bytes_received;
    result.bytes_received += buffer_.HandleResponse(
        buf + offset, n - offset,
        StealMutableContent(*response.mutable_checksummed_data()));
  }
  if (response.has_object_checksums()) {
    auto const& checksums = response.object_checksums();
    if (checksums.has_crc32c()) {
      result.hashes =
          Merge(std::move(result.hashes),
                storage::internal::HashValues{
                    storage_internal::Crc32cFromProto(checksums.crc32c()), {}});
    }
    if (!checksums.md5_hash().empty()) {
      result.hashes =
          Merge(std::move(result.hashes),
                storage::internal::HashValues{
                    {}, storage_internal::MD5FromProto(checksums.md5_hash())});
    }
  }
  if (response.has_metadata()) {
    auto const& metadata = response.metadata();
    result.generation = result.generation.value_or(metadata.generation());
    result.metageneration =
        result.metageneration.value_or(metadata.metageneration());
    result.storage_class =
        result.storage_class.value_or(metadata.storage_class());
    result.size = result.size.value_or(metadata.size());
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
