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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CREDENTIALS_H_

#include "google/cloud/storage/version.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Base class for the credential objects.
 */
class Credentials {
 public:
  virtual ~Credentials() = default;

  /**
   * Return the value for the Authorization header in HTTP requests.
   */
  virtual std::string AuthorizationHeader() = 0;
};

std::shared_ptr<Credentials> GoogleDefaultCredentials();

/**
 * Credentials to access Google Cloud Storage anonymously.
 *
 * This is only useful in two cases: (a) in testing, where you want to access
 * a test bench without having to worry about authentication or SSL setup, and
 * (b) when accessing publicly readable buckets or objects without credentials.
 */
class InsecureCredentials : public storage::Credentials {
 public:
  InsecureCredentials() = default;

  std::string AuthorizationHeader() override;
};

inline std::shared_ptr<Credentials> CreateInsecureCredentials() {
  return std::make_shared<InsecureCredentials>();
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CREDENTIALS_H_
