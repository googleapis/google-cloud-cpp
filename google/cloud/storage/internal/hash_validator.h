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
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Defines the interface to check hash values during uploads and downloads.
 */
class HashValidator {
 public:
  virtual ~HashValidator() = default;

  /// Update the actual hash value with some portion of the data.
  virtual void Update(std::string const& payload) = 0;

  /// Update the expected hash value based on a response header.
  virtual void ProcessHeader(std::string const& key,
                             std::string const& value) = 0;

  struct Result {
    /// The value reported by the server, based on the calls to ProcessHeader().
    std::string received;
    /// The value computed locally, based on the calls to Update().
    std::string computed;
  };

  /**
   * Compute the final hash values.
   *
   * @returns The two hashes, expected is the locally computed value, actual is
   *   the value reported by the service. Note that this can be empty for
   *   validators that disable validation.
   *
   * @throws google::cloud::storage::HashValidation
   */
  virtual Result Finish(std::string const& msg) && = 0;
};

/**
 * A validator that does not validate.
 */
class NullHashValidator : public HashValidator {
 public:
  NullHashValidator() = default;

  void Update(std::string const& payload) override {}
  void ProcessHeader(std::string const& key,
                     std::string const& value) override {}
  Result Finish(std::string const& msg) && override { return Result{}; }
};

/**
 * A validator based on MD5 hashes.
 */
class MD5HashValidator : public HashValidator {
 public:
  MD5HashValidator();

  MD5HashValidator(MD5HashValidator const&) = delete;
  MD5HashValidator& operator=(MD5HashValidator const&) = delete;

  void Update(std::string const& payload) override;
  void ProcessHeader(std::string const& key, std::string const& value) override;
  Result Finish(std::string const& msg) && override;

 private:
  MD5_CTX context_;
  std::string received_hash_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H_
