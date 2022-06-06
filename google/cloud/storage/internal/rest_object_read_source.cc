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

#include "google/cloud/storage/internal/rest_object_read_source.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

std::string RestExtractHashValue(std::string const& hash_header,
                                 std::string const& hash_key) {
  auto const pos = hash_header.find(hash_key);
  if (pos == std::string::npos) return {};
  auto const start = pos + hash_key.size();
  auto const end = hash_header.find(',', start);
  if (end == std::string::npos) {
    return hash_header.substr(start);
  }
  return hash_header.substr(start, end - start);
}

ReadSourceResult MakeRestReadResult(std::size_t bytes_received,
                                    HttpResponse response) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto r = ReadSourceResult{bytes_received, std::move(response)};
  auto const end = r.response.headers.end();
  auto f = r.response.headers.find("x-goog-generation");
  if (f != end && !r.generation) r.generation = std::stoll(f->second);
  f = r.response.headers.find("x-goog-metageneration");
  if (f != end && !r.metageneration) r.metageneration = std::stoll(f->second);
  f = r.response.headers.find("x-goog-storage-class");
  if (f != end && !r.storage_class) r.storage_class = f->second;
  f = r.response.headers.find("x-goog-stored-content-length");
  if (f != end && !r.size) r.size = std::stoull(f->second);
  f = r.response.headers.find("x-guploader-response-body-transformations");
  if (f != end && !r.transformation) r.transformation = f->second;

  // Prefer "Content-Range" over "Content-Length" because the former works for
  // ranged downloads.
  f = r.response.headers.find("content-range");
  if (f != end && !r.size) {
    auto const l = f->second.find_last_of('/');
    if (l != std::string::npos) r.size = std::stoll(f->second.substr(l + 1));
  }
  f = r.response.headers.find("content-length");
  if (f != end && !r.size) r.size = std::stoll(f->second);

  // x-goog-hash is special in that it does appear multiple times in the
  // headers, and we want to accumulate all the values.
  auto const range = r.response.headers.equal_range("x-goog-hash");
  for (auto i = range.first; i != range.second; ++i) {
    HashValues h;
    h.crc32c = RestExtractHashValue(i->second, "crc32c=");
    h.md5 = RestExtractHashValue(i->second, "md5=");
    r.hashes = Merge(std::move(r.hashes), std::move(h));
  }
  return r;
}

}  // namespace

RestObjectReadSource::RestObjectReadSource(
    std::unique_ptr<google::cloud::rest_internal::RestResponse> response)
    : status_code_(response->StatusCode()),
      headers_(response->Headers()),
      payload_(std::move(*response).ExtractPayload()) {}

StatusOr<HttpResponse> RestObjectReadSource::Close() {
  if (!payload_) {
    return Status(StatusCode::kFailedPrecondition, "Connection not open.");
  }
  payload_.reset();
  return HttpResponse{status_code_, {}, headers_};
}

StatusOr<ReadSourceResult> RestObjectReadSource::Read(char* buf,
                                                      std::size_t n) {
  if (!payload_) {
    return Status(StatusCode::kFailedPrecondition, "Connection not open.");
  }

  if (status_code_ >= google::cloud::rest_internal::kMinNotSuccess) {
    return MakeRestReadResult(0, HttpResponse{status_code_, {}, headers_});
  }

  auto read = payload_->Read(absl::MakeSpan(buf, n));
  if (!read.ok()) return read.status();

  HttpResponse h;
  if (payload_->HasUnreadData()) {
    h.status_code = HttpStatusCode::kContinue;
  } else {
    h.status_code = status_code_;
  }
  h.headers = headers_;
  return MakeRestReadResult(*read, std::move(h));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
