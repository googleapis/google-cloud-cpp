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
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ObjectMetadata;
namespace internal {
/**
 * A validator that does not validate.
 */
class NullHashValidator : public HashValidator {
 public:
  NullHashValidator() = default;

  std::string Name() const override { return "null"; }
  void ProcessMetadata(ObjectMetadata const&) override {}
  void ProcessHeader(std::string const&, std::string const&) override {}
  Result Finish(HashValues computed) && override;
};

/**
 * A composite validator.
 */
class CompositeValidator : public HashValidator {
 public:
  CompositeValidator(std::unique_ptr<HashValidator> a,
                     std::unique_ptr<HashValidator> b)
      : a_(std::move(a)), b_(std::move(b)) {}

  std::string Name() const override { return "composite"; }
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish(HashValues computed) && override;

 private:
  std::unique_ptr<HashValidator> a_;
  std::unique_ptr<HashValidator> b_;
};

/**
 * A validator based on MD5 hashes.
 */
class MD5HashValidator : public HashValidator {
 public:
  MD5HashValidator() = default;

  MD5HashValidator(MD5HashValidator const&) = delete;
  MD5HashValidator& operator=(MD5HashValidator const&) = delete;

  std::string Name() const override { return "md5"; }
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish(HashValues computed) && override;

 private:
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
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish(HashValues computed) && override;

 private:
  std::string received_hash_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_IMPL_H
