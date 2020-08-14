// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_KMS_KEY_NAME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_KMS_KEY_NAME_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

class KmsKeyName;
/**
 * Constructs a `KmsKeyName` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<KmsKeyName> MakeKmsKeyName(std::string full_name);

/**
 * This class identifies a Google Cloud KMS Key
 *
 * A KMS key is identified by its `project_id`, `location`, `key_ring`,
 * and `kms_key_name`.
 *
 * @note this class makes no effort to validate the components of the key,
 *     It is the application's responsibility to provide a valid project id,
 *     location, key ring, and KMS key name. Passing invalid values will not
 *     be checked until the key is used in a RPC.
 *
 * See https://cloud.google.com/kms/docs for more information on KMS.
 */
class KmsKeyName {
 public:
  /**
   * Constructs a KmsKeyName object identified by the given @p project_id,
   * @p location, @p key_ring, and @p kms_key_name.
   */
  KmsKeyName(std::string const& project_id, std::string const& location,
             std::string const& key_ring, std::string const& kms_key_name);

  /// @name Copy and move
  //@{
  KmsKeyName(KmsKeyName const&) = default;
  KmsKeyName& operator=(KmsKeyName const&) = default;
  KmsKeyName(KmsKeyName&&) = default;
  KmsKeyName& operator=(KmsKeyName&&) = default;
  //@}

  /**
   * Returns the fully qualified KMS Key name as a string of the form:
   * "projects/<project>/locations/<location>/keyRings/<key_ring>/cryptoKeys\
   * /<kms_key_name>".
   */
  std::string const& FullName() const { return full_name_; }

  /// @name Equality operators
  //@{
  friend bool operator==(KmsKeyName const& a, KmsKeyName const& b);
  friend bool operator!=(KmsKeyName const& a, KmsKeyName const& b) {
    return !(a == b);
  }
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream& os, KmsKeyName const& key);

  friend StatusOr<KmsKeyName> MakeKmsKeyName(std::string full_name);

 private:
  explicit KmsKeyName(std::string full_name)
      : full_name_(std::move(full_name)) {}

  std::string full_name_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_KMS_KEY_NAME_H
