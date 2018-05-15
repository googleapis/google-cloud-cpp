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

#include "storage/client/credentials.h"
#include "storage/client/internal/jwt_credentials.h"
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
#ifdef _WIN32
char const CREDENTIALS_ENV_VAR[] = "APPDATA";
#else
char const CREDENTIALS_ENV_VAR[] = "HOME";
#endif

std::string const& GoogleCredentialsSuffix() {
#ifdef _WIN32
  static std::string const suffix =
      "/gcloud/application_default_credentials.json";
#else
  static std::string const suffix =
      "/.config/gcloud/application_default_credentials.json";
#endif
  return suffix;
}
}  // anonymous namespace

namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::shared_ptr<Credentials> GoogleDefaultCredentials() {
  // There are probably more efficient ways to do this, but meh, the strings
  // are typically short, and this does not happen that often.
  std::string const root = std::getenv(CREDENTIALS_ENV_VAR);
  std::ifstream is(root + GoogleCredentialsSuffix());
  std::string jwt(std::istreambuf_iterator<char>{is}, {});
  return std::make_shared<storage::internal::JwtCredentials<>>(jwt);
}

}  // namespace STORAGE_CLIENT_NS
}  // storage
