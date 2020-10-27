// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/grpc_error_delegate.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

GrpcObjectReadSource::~GrpcObjectReadSource() {
  // clang-tidy is complaining about code in the gRPC implementation. It seems
  // that it is *possible* for gRPC to call a function through a null pointer
  // I hesitate to disable the warning because it seems *really* useful, but I
  // could not find another way to disable it that putting this obscure comment
  // here.
  //
  // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
  if (stream_) {
    (void)stream_->Finish();
    stream_ = nullptr;
  }
}

StatusOr<HttpResponse> GrpcObjectReadSource::Close() {
  if (stream_) {
    // Cancel the streaming RPC so we can close the stream early and
    // ignore any errors (kCancelled is likely).
    context_.TryCancel();
    (void)stream_->Finish();
    stream_ = nullptr;
  }
  if (!status_.ok()) {
    return status_;
  }
  return HttpResponse{HttpStatusCode::kOk, {}, {}};
}

/// Read more data from the download, returning any HTTP headers and error
/// codes.
StatusOr<ReadSourceResult> GrpcObjectReadSource::Read(char* buf,
                                                      std::size_t n) {
  std::multimap<std::string, std::string> headers;
  std::size_t offset = 0;
  auto update_buf = [&offset, buf, n](std::string source) {
    if (source.empty()) {
      return source;
    }
    auto const nbytes = std::min(n - offset, source.size());
    std::copy(source.data(), source.data() + nbytes, buf + offset);
    offset += nbytes;
    source.erase(0, nbytes);
    return source;
  };

  spill_ = update_buf(std::move(spill_));

  while (offset < n && stream_) {
    google::storage::v1::GetObjectMediaResponse response;
    bool success = stream_->Read(&response);

    // The google.storage.v1.Storage documentation says this field can be empty.
    if (response.has_checksummed_data()) {
      spill_ = update_buf(
          std::move(*response.mutable_checksummed_data()->mutable_content()));
    }
    if (response.has_object_checksums()) {
      auto const& checksums = response.object_checksums();
      if (checksums.has_crc32c()) {
        headers.emplace("x-goog-hash", "crc32c=" + GrpcClient::Crc32cFromProto(
                                                       checksums.crc32c()));
      }
      if (!checksums.md5_hash().empty()) {
        headers.emplace("x-goog-hash", "md5=" + GrpcClient::MD5FromProto(
                                                    checksums.md5_hash()));
      }
    }
    if (!success) {
      status_ = google::cloud::MakeStatusFromRpcError(stream_->Finish());
      stream_ = nullptr;
    }
  }

  if (offset != 0) {
    return ReadSourceResult{
        offset,
        HttpResponse{HttpStatusCode::kContinue, {}, std::move(headers)}};
  }
  if (status_.ok()) {
    // The stream was closed successfully, but there is no more data, cannot
    // return a "OK" Status via a `StatusOr` need to provide some value.
    return ReadSourceResult{
        0, HttpResponse{HttpStatusCode::kOk, {}, std::move(headers)}};
  }

  return status_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
