// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ENCRYPTION_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ENCRYPTION_CONFIG_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/kms_key_name.h"
#include "absl/types/variant.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Use the per-operation default encryption:
 *  - for `CreateDatabase()` use Google default encryption,
 *  - for `CreateBackup()` use the encryption of the source database,
 *  - for `RestoreDatabase()` use the encryption of the source backup.
 */
class DefaultEncryption {};

/**
 * Use Google default encryption.
 */
class GoogleEncryption {};

/**
 * Use encryption with `encryption_key`.
 */
class CustomerManagedEncryption {
 public:
  explicit CustomerManagedEncryption(KmsKeyName encryption_key)
      : encryption_key_(std::move(encryption_key)) {}

  KmsKeyName const& encryption_key() const { return encryption_key_; }

 private:
  KmsKeyName encryption_key_;
};

/**
 * Specify the encryption configuration for any of the following operations:
 *  - `DatabaseAdminClient::CreateDatabase()`
 *  - `DatabaseAdminClient::CreateBackup()`
 *  - `DatabaseAdminClient::RestoreDatabase()`
 */
using EncryptionConfig = absl::variant<DefaultEncryption, GoogleEncryption,
                                       CustomerManagedEncryption>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ENCRYPTION_CONFIG_H
