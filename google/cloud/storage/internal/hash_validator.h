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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H_

#include "google/cloud/storage/version.h"
#include <openssl/md5.h>
#include <memory>
#include <string>
#include <utility>

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
  virtual void Update(std::string const& payload) = 0;

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

  /**
   * Raise an exception if the hashes do not match.
   *
   * @throws google::cloud::storage::HashValidation
   */
  static void CheckResult(std::string const& msg, Result const& result);

  /**
   * Call Finish() on the validator and check the result.
   */
  static Result FinishAndCheck(std::string const& msg,
                               HashValidator&& validator);
};

/**
 * A validator that does not validate.
 */
class NullHashValidator : public HashValidator {
 public:
  NullHashValidator() = default;

  std::string Name() const override { return "null"; }
  void Update(std::string const& payload) override {}
  void ProcessMetadata(ObjectMetadata const& meta) override {}
  void ProcessHeader(std::string const& key,
                     std::string const& value) override {}
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
  void Update(std::string const& payload) override;
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish() && override;

 private:
  std::unique_ptr<HashValidator> left_;
  std::unique_ptr<HashValidator> right_;
};

/**
 * A validator based on MD5 hashes.
 */
class MD5HashValidator : public HashValidator {
 public:
  MD5HashValidator();

  MD5HashValidator(MD5HashValidator const&) = delete;
  MD5HashValidator& operator=(MD5HashValidator const&) = delete;

  std::string Name() const override { return "md5"; }
  void Update(std::string const& payload) override;
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
  Crc32cHashValidator();

  Crc32cHashValidator(Crc32cHashValidator const&) = delete;
  Crc32cHashValidator& operator=(Crc32cHashValidator const&) = delete;

  std::string Name() const override { return "crc32c"; }
  void Update(std::string const& payload) override;
  void ProcessMetadata(ObjectMetadata const& meta) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish() && override;

 private:
  std::uint32_t current_;
  std::string received_hash_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H_
