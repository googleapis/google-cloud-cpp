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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_IMPL_H

#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/version.h"
#include <openssl/md5.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ObjectMetadata;
namespace internal {
/**
 * A validator based on MD5 hashes.
 */
class MD5HashValidator : public HashValidator {
 public:
  MD5HashValidator();

  MD5HashValidator(MD5HashValidator const&) = delete;
  MD5HashValidator& operator=(MD5HashValidator const&) = delete;

  std::string Name() const override { return "md5"; }
  void Update(char const* buf, std::size_t n) override;
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish() && override;

 private:
  MD5_CTX context_;
  std::string received_hash_;
};

/**
 * A validator based on CRC32C checksums.
 */
class Crc32cHashValidator : public HashValidator {
 public:
  Crc32cHashValidator() = default;

  Crc32cHashValidator(Crc32cHashValidator const&) = delete;
  Crc32cHashValidator& operator=(Crc32cHashValidator const&) = delete;

  std::string Name() const override { return "crc32c"; }
  void Update(char const* buf, std::size_t n) override;
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish() && override;

 private:
  std::uint32_t current_{0};
  std::string received_hash_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_IMPL_H
