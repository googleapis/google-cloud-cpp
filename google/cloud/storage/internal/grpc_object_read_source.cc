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
#include "absl/functional/function_ref.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using HeadersMap = std::multimap<std::string, std::string>;

HeadersMap MakeHeadersFromChecksums(
    google::storage::v1::ObjectChecksums const& checksums) {
  HeadersMap headers;
  if (checksums.has_crc32c()) {
    headers.emplace("x-goog-hash", "crc32c=" + GrpcClient::Crc32cFromProto(
                                                   checksums.crc32c()));
  }
  if (!checksums.md5_hash().empty()) {
    headers.emplace("x-goog-hash",
                    "md5=" + GrpcClient::MD5FromProto(checksums.md5_hash()));
  }
  return headers;
}
}  // namespace

GrpcObjectReadSource::GrpcObjectReadSource(std::unique_ptr<StreamingRpc> stream)
    : stream_(std::move(stream)) {}

StatusOr<HttpResponse> GrpcObjectReadSource::Close() {
  if (stream_) stream_ = nullptr;
  if (!status_.ok()) return status_;
  return HttpResponse{HttpStatusCode::kOk, {}, {}};
}

/// Read more data from the download, returning any HTTP headers and error
/// codes.
StatusOr<ReadSourceResult> GrpcObjectReadSource::Read(char* buf,
                                                      std::size_t n) {
  std::size_t offset = 0;
  auto update_buf = [&offset, buf, n](std::string source) {
    if (source.empty()) return source;
    auto const nbytes = std::min(n - offset, source.size());
    std::copy(source.data(), source.data() + nbytes, buf + offset);
    offset += nbytes;
    source.erase(0, nbytes);
    return source;
  };
  struct Visitor {
    GrpcObjectReadSource& self;
    absl::FunctionRef<std::string(std::string)> update_buf;

    HeadersMap operator()(Status s) {
      // A status, whether success or failure, closes the stream.
      self.status_ = std::move(s);
      auto metadata = self.stream_->GetRequestMetadata();
      self.stream_ = nullptr;
      return metadata;
    }
    HeadersMap operator()(
        google::storage::v1::GetObjectMediaResponse response) {
      // The google.storage.v1.Storage documentation says this field can be
      // empty.
      if (response.has_checksummed_data()) {
        self.spill_ =
            update_buf(std::string(  // Sometimes protobuf bytes are not strings
                std::move(
                    *response.mutable_checksummed_data()->mutable_content())));
      }
      if (self.checksums_known_) return {};
      if (!response.has_object_checksums()) return {};
      self.checksums_known_ = true;
      return MakeHeadersFromChecksums(response.object_checksums());
    }
  };

  spill_ = update_buf(std::move(spill_));
  HeadersMap headers;
  while (offset < n && stream_) {
    auto v = stream_->Read();
    auto h = absl::visit(Visitor{*this, update_buf}, std::move(v));
    headers.insert(h.begin(), h.end());
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
