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

#include "google/cloud/storage/internal/hash_values.h"
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

  /// Update the received hash value based on a ObjectMetadata response.
  virtual void ProcessMetadata(ObjectMetadata const& meta) = 0;

  /// Update the received hash value based on a response header.
  virtual void ProcessHashValues(HashValues const& hashes) = 0;

  struct Result {
    /// The value reported by the server, based on the calls to ProcessHeader().
    HashValues received;
    /// The value computed locally, based on the calls to Update().
    HashValues computed;
    /// A flag indicating of this is considered a mismatch based on the rules
    /// for the validator.
    bool is_mismatch = false;

    Result() = default;
    Result(HashValues r, HashValues c, bool m)
        : received(std::move(r)), computed(std::move(c)), is_mismatch(m) {}
  };

  /**
   * Compute the final hash values.
   *
   * @returns The two hashes, expected is the locally computed value, actual is
   *   the value reported by the service. Note that this can be empty for
   *   validators that disable validation.
   */
  virtual Result Finish(HashValues computed) && = 0;
};

std::unique_ptr<HashValidator> CreateNullHashValidator();

class ReadObjectRangeRequest;
class ResumableUploadRequest;

/// Create a hash validator configured by @p request.
std::unique_ptr<HashValidator> CreateHashValidator(
    ReadObjectRangeRequest const& request);

/// Create a hash validator configured by @p request.
std::unique_ptr<HashValidator> CreateHashValidator(
    ResumableUploadRequest const& request);

/// Received hashes as a string.
std::string FormatReceivedHashes(HashValidator::Result const& result);

/// Computed hashes as a string.
std::string FormatComputedHashes(HashValidator::Result const& result);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALIDATOR_H
