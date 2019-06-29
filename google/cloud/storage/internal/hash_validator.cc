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
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/object_metadata.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

void CompositeValidator::Update(char const* buf, std::size_t n) {
  left_->Update(buf, n);
  right_->Update(buf, n);
}

void CompositeValidator::ProcessMetadata(ObjectMetadata const& meta) {
  left_->ProcessMetadata(meta);
  right_->ProcessMetadata(meta);
}

void CompositeValidator::ProcessHeader(std::string const& key,
                                       std::string const& value) {
  left_->ProcessHeader(key, value);
  right_->ProcessHeader(key, value);
}

HashValidator::Result CompositeValidator::Finish() && {
  auto left_result = std::move(*left_).Finish();
  auto right_result = std::move(*right_).Finish();

  auto received = left_->Name() + "=" + left_result.received;
  received += "," + right_->Name() + "=" + right_result.received;
  auto computed = left_->Name() + "=" + left_result.computed;
  computed += "," + right_->Name() + "=" + right_result.computed;
  bool is_mismatch = left_result.is_mismatch || right_result.is_mismatch;
  return Result{std::move(received), std::move(computed), is_mismatch};
}

std::unique_ptr<HashValidator> CreateHashValidator(bool disable_md5,
                                                   bool disable_crc32c) {
  if (disable_md5 && disable_crc32c) {
    return google::cloud::internal::make_unique<NullHashValidator>();
  }
  if (disable_md5) {
    return google::cloud::internal::make_unique<Crc32cHashValidator>();
  }
  if (disable_crc32c) {
    return google::cloud::internal::make_unique<MD5HashValidator>();
  }
  return google::cloud::internal::make_unique<CompositeValidator>(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
}

std::unique_ptr<HashValidator> CreateHashValidator(
    ReadObjectRangeRequest const& request) {
  if (request.RequiresRangeHeader()) {
    return google::cloud::internal::make_unique<NullHashValidator>();
  }
  return CreateHashValidator(request.HasOption<DisableMD5Hash>(),
                             request.HasOption<DisableCrc32cChecksum>());
}

std::unique_ptr<HashValidator> CreateHashValidator(
    ResumableUploadRequest const& request) {
  return CreateHashValidator(request.HasOption<DisableMD5Hash>(),
                             request.HasOption<DisableCrc32cChecksum>());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
