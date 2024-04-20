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

#include "google/cloud/storage/internal/async/reader_connection_impl.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

future<AsyncReaderConnectionImpl::ReadResponse>
AsyncReaderConnectionImpl::Read() {
  return impl_->Read().then([this](auto f) { return OnRead(f.get()); });
}

RpcMetadata AsyncReaderConnectionImpl::GetRequestMetadata() {
  return impl_->GetRequestMetadata();
}

future<AsyncReaderConnectionImpl::ReadResponse>
AsyncReaderConnectionImpl::OnRead(absl::optional<ProtoPayload> r) {
  if (!r) return DoFinish();
  auto response = *std::move(r);
  auto hash =
      hash_function_->Update(offset_, GetContent(response.checksummed_data()),
                             response.checksummed_data().crc32c());
  if (!hash.ok()) {
    (void)DoFinish();
    return make_ready_future(ReadResponse(std::move(hash)));
  }
  auto result = ReadPayloadImpl::Make(
      StealMutableContent(*response.mutable_checksummed_data()));
  if (response.has_object_checksums()) {
    storage::internal::HashValues hashes;
    auto const& checksums = response.object_checksums();
    if (checksums.has_crc32c()) {
      hashes =
          Merge(std::move(hashes),
                storage::internal::HashValues{
                    storage_internal::Crc32cFromProto(checksums.crc32c()), {}});
    }
    if (!checksums.md5_hash().empty()) {
      hashes =
          Merge(std::move(hashes),
                storage::internal::HashValues{
                    {}, storage_internal::MD5FromProto(checksums.md5_hash())});
    }
    ReadPayloadImpl::SetObjectHashes(result, std::move(hashes));
  }
  if (response.has_metadata()) {
    result.set_metadata(std::move(*response.mutable_metadata()));
  }
  if (response.has_content_range()) {
    result.set_offset(response.content_range().start());
  } else {
    result.set_offset(offset_);
  }
  offset_ = result.offset() + result.size();
  return make_ready_future(ReadResponse(std::move(result)));
}

future<AsyncReaderConnectionImpl::ReadResponse>
AsyncReaderConnectionImpl::DoFinish() {
  return impl_->Finish().then([](auto f) { return ReadResponse(f.get()); });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
