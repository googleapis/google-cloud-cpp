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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H

#include "google/cloud/storage/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ObjectMetadata;
namespace internal {
/**
 * Defines the interface to check hash values during uploads and downloads.
 */
class HashValidator {
 public:
  virtual ~HashValidator() = default;

  /// A short string that names the validator when composing results.
  virtual std::string Name() const = 0;

  /// Update the computed hash value with some portion of the data.
  virtual void Update(char const* buf, std::size_t n) = 0;

  /// Update the received hash value based on a ObjectMetadata response.
  virtual void ProcessMetadata(ObjectMetadata const& meta) = 0;

  /// Update the received hash value based on a response header.
  virtual void ProcessHeader(std::string const& key,
                             std::string const& value) = 0;

  struct Result {
    /// The value reported by the server, based on the calls to ProcessHeader().
    std::string received;
    /// The value computed locally, based on the calls to Update().
    std::string computed;
    /// A flag indicating of this is considered a mismatch based on the rules
    /// for the validator.
    bool is_mismatch;
  };

  /**
   * Compute the final hash values.
   *
   * @returns The two hashes, expected is the locally computed value, actual is
   *   the value reported by the service. Note that this can be empty for
   *   validators that disable validation.
   */
  virtual Result Finish() && = 0;
};

/**
 * A validator that does not validate.
 */
class NullHashValidator : public HashValidator {
 public:
  NullHashValidator() = default;

  std::string Name() const override { return "null"; }
  void Update(char const*, std::size_t) override {}
  void ProcessMetadata(ObjectMetadata const&) override {}
  void ProcessHeader(std::string const&, std::string const&) override {}
  Result Finish() && override { return Result{}; }
};

/**
 * A composite validator.
 */
class CompositeValidator : public HashValidator {
 public:
  CompositeValidator(std::unique_ptr<HashValidator> left,
                     std::unique_ptr<HashValidator> right)
      : left_(std::move(left)), right_(std::move(right)) {}

  std::string Name() const override { return "composite"; }
  void Update(char const* buf, std::size_t n) override;
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish() && override;

 private:
  std::unique_ptr<HashValidator> left_;
  std::unique_ptr<HashValidator> right_;
};

class ReadObjectRangeRequest;
class ResumableUploadRequest;

/**
 * @{
 * The requests accepted by `CreateHashValidator` can be configured with the
 * `DisableMD5Hash` and `DisableCrc32Checksum` options. You must explicitly
 * pass `DisableCrc32Checksum` to disable Crc32cChecksum. MD5Hash is disabled by
 * default. You must explicitly pass `EnableMD5Hash()` to enable MD5Hash.
 *
 * The valid combinations are:
 * - If neither `DisableMD5Hash(true)` nor `DisableCrc32cChecksum(true)` are
 *   provided, then only Crc32cChecksum is used.
 * - If only `DisableMD5Hash(true)` is provided, then only Crc32cChecksum is
 *   used.
 * - If only `DisableCrc32c(true)` is provided, then neither are used.
 * - If both `DisableMD5Hash(true)` and `DisableCrc32cChecksum(true)` are
 *   provided, then neither are used.
 *
 * Specifying the option with `false` or no argument (default constructor) has
 * the same effect as not passing the option at all.
 */
/// Create a hash validator configured by @p request.
std::unique_ptr<HashValidator> CreateHashValidator(
    ReadObjectRangeRequest const& request);

/// Create a hash validator configured by @p request.
std::unique_ptr<HashValidator> CreateHashValidator(
    ResumableUploadRequest const& request);
/* @} */

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H
