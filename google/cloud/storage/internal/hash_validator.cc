// Copyright 2018 Google LLC
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

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/checksum_helpers.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

std::unique_ptr<HashValidator> CreateHashValidator(bool disable_md5,
                                                   bool disable_crc32c) {
  if (disable_md5 && disable_crc32c) {
    return std::make_unique<NullHashValidator>();
  }
  if (disable_md5) {
    return std::make_unique<Crc32cHashValidator>();
  }
  if (disable_crc32c) {
    return std::make_unique<MD5HashValidator>();
  }
  return std::make_unique<CompositeValidator>(
      std::make_unique<Crc32cHashValidator>(),
      std::make_unique<MD5HashValidator>());
}

}  // namespace

std::unique_ptr<HashValidator> CreateNullHashValidator() {
  return std::make_unique<NullHashValidator>();
}

std::unique_ptr<HashValidator> CreateHashValidator(
    ReadObjectRangeRequest const& request) {
  if (request.RequiresRangeHeader()) return CreateNullHashValidator();

  auto const settings = GetDownloadChecksumSettings(
      request, google::cloud::internal::CurrentOptions());
  auto const disable_md5 = settings.md5;
  auto const disable_crc32c = settings.crc32c;
  return CreateHashValidator(disable_md5, disable_crc32c);
}

std::unique_ptr<HashValidator> CreateHashValidator(
    ResumableUploadRequest const& request) {
  auto const settings = GetUploadChecksumSettings(
      request, google::cloud::internal::CurrentOptions());
  auto const disable_md5 = settings.md5;
  auto const disable_crc32c = settings.crc32c;
  return CreateHashValidator(disable_md5, disable_crc32c);
}

std::string FormatReceivedHashes(HashValidator::Result const& result) {
  return Format(result.received);
}

std::string FormatComputedHashes(HashValidator::Result const& result) {
  return Format(result.computed);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#include "google/cloud/internal/diagnostics_pop.inc"
