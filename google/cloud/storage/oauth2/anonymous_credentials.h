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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_ANONYMOUS_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_ANONYMOUS_CREDENTIALS_H_

#include "google/cloud/storage/oauth2/credentials.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/**
 * Defines credentials to access Google Cloud Storage anonymously.
 *
 * This is only useful in two cases: (a) in testing, where you want to access
 * a test bench without having to worry about authentication or SSL setup, and
 * (b) when accessing publicly readable buckets or objects without credentials.
 */
class AnonymousCredentials : public Credentials {
 public:
  AnonymousCredentials() = default;

  std::string AuthorizationHeader() override;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_ANONYMOUS_CREDENTIALS_H_
