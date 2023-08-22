// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/object_requests.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {
std::unique_ptr<HashFunction> CreateHashFunction(bool disable_crc32c,
                                                 bool disable_md5) {
  if (disable_md5 && disable_crc32c) {
    return std::make_unique<NullHashFunction>();
  }
  if (disable_md5) return std::make_unique<Crc32cHashFunction>();
  if (disable_crc32c) return std::make_unique<MD5HashFunction>();
  return std::make_unique<CompositeFunction>(
      std::make_unique<Crc32cHashFunction>(),
      std::make_unique<MD5HashFunction>());
}

template <typename Request>
std::unique_ptr<HashFunction> CreateUploadHashFunction(Request const& request) {
  auto crc32c = std::unique_ptr<HashFunction>();
  auto crc32c_value =
      request.template GetOption<Crc32cChecksumValue>().value_or("");
  if (!crc32c_value.empty()) {
    crc32c = std::make_unique<PrecomputedHashFunction>(
        HashValues{/*.crc32c=*/std::move(crc32c_value), /*md5=*/{}});
  } else if (!request.template GetOption<DisableCrc32cChecksum>().value_or(
                 false)) {
    crc32c = std::make_unique<Crc32cHashFunction>();
  }

  auto md5 = std::unique_ptr<HashFunction>();
  auto md5_value = request.template GetOption<MD5HashValue>().value_or("");
  if (!md5_value.empty()) {
    md5 = std::make_unique<PrecomputedHashFunction>(
        HashValues{/*.crc32c=*/{}, /*.md5=*/std::move(md5_value)});
  } else if (!request.template GetOption<DisableMD5Hash>().value_or(false)) {
    md5 = std::make_unique<MD5HashFunction>();
  }

  if (!crc32c && !md5) return std::make_unique<NullHashFunction>();
  if (!crc32c) return md5;
  if (!md5) return crc32c;

  return std::make_unique<CompositeFunction>(std::move(crc32c), std::move(md5));
}

}  // namespace

std::unique_ptr<HashFunction> CreateNullHashFunction() {
  return std::make_unique<NullHashFunction>();
}

std::unique_ptr<HashFunction> CreateHashFunction(
    ReadObjectRangeRequest const& request) {
  if (request.RequiresRangeHeader()) return CreateNullHashFunction();
  return CreateHashFunction(
      request.GetOption<DisableCrc32cChecksum>().value_or(false),
      request.GetOption<DisableMD5Hash>().value_or(false));
}

std::unique_ptr<HashFunction> CreateHashFunction(
    ResumableUploadRequest const& request) {
  if (!request.GetOption<UseResumableUploadSession>().value_or("").empty()) {
    // Resumed sessions cannot include a hash function because the hash state
    // for previous values is lost.
    return CreateNullHashFunction();
  }
  return CreateUploadHashFunction(request);
}

std::unique_ptr<HashFunction> CreateHashFunction(
    InsertObjectMediaRequest const& request) {
  return CreateUploadHashFunction(request);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
