// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/object_metadata.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

std::unique_ptr<HashValidator> CreateHashValidator(bool disable_md5,
                                                   bool disable_crc32c) {
  if (disable_md5 && disable_crc32c) {
    return absl::make_unique<NullHashValidator>();
  }
  if (disable_md5) {
    return absl::make_unique<Crc32cHashValidator>();
  }
  if (disable_crc32c) {
    return absl::make_unique<MD5HashValidator>();
  }
  return absl::make_unique<CompositeValidator>(
      absl::make_unique<Crc32cHashValidator>(),
      absl::make_unique<MD5HashValidator>());
}

}  // namespace

std::unique_ptr<HashValidator> CreateNullHashValidator() {
  return absl::make_unique<NullHashValidator>();
}

std::unique_ptr<HashValidator> CreateHashValidator(
    ReadObjectRangeRequest const& request) {
  if (request.RequiresRangeHeader()) return CreateNullHashValidator();

  // `DisableMD5Hash`'s default value is `true`.
  auto disable_md5 = request.GetOption<DisableMD5Hash>().value_or(false);
  auto disable_crc32c =
      request.GetOption<DisableCrc32cChecksum>().value_or(false);
  return CreateHashValidator(disable_md5, disable_crc32c);
}

std::unique_ptr<HashValidator> CreateHashValidator(
    ResumableUploadRequest const& request) {
  // `DisableMD5Hash`'s default value is `true`.
  auto disable_md5 = request.GetOption<DisableMD5Hash>().value_or(false);
  auto disable_crc32c =
      request.GetOption<DisableCrc32cChecksum>().value_or(false);
  return CreateHashValidator(disable_md5, disable_crc32c);
}

std::string FormatReceivedHashes(HashValidator::Result const& result) {
  return Format(result.received);
}

std::string FormatComputedHashes(HashValidator::Result const& result) {
  return Format(result.computed);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
