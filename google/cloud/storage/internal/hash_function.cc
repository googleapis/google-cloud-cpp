// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
std::unique_ptr<HashFunction> CreateHashFunction(bool disable_crc32c,
                                                 bool disable_md5) {
  if (disable_md5 && disable_crc32c) {
    return absl::make_unique<NullHashFunction>();
  }
  if (disable_md5) return absl::make_unique<Crc32cHashFunction>();
  if (disable_crc32c) return absl::make_unique<MD5HashFunction>();
  return absl::make_unique<CompositeFunction>(
      absl::make_unique<Crc32cHashFunction>(),
      absl::make_unique<MD5HashFunction>());
}
}  // namespace

std::unique_ptr<HashFunction> CreateNullHashFunction() {
  return absl::make_unique<NullHashFunction>();
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
  // A non-empty Crc32cChecksumValue implies that the application provided a
  // hash, and we should not compute our own.
  return CreateHashFunction(
      request.GetOption<DisableCrc32cChecksum>().value_or(false) ||
          !request.GetOption<Crc32cChecksumValue>().value_or("").empty(),
      request.GetOption<DisableMD5Hash>().value_or(false) ||
          !request.GetOption<MD5HashValue>().value_or("").empty());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
