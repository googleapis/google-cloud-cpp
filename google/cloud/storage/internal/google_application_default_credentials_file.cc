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

#include "google/cloud/storage/internal/google_application_default_credentials_file.h"
#include "google/cloud/internal/throw_delegate.h"
#include <sstream>

namespace {
#ifdef _WIN32
char const CREDENTIALS_HOME_VAR[] = "APPDATA";
#else
char const CREDENTIALS_HOME_VAR[] = "HOME";
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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
char const* GoogleApplicationDefaultCredentialsHomeVariable() {
  return CREDENTIALS_HOME_VAR;
}

std::string GoogleApplicationDefaultCredentialsFile() {
  auto override_value = std::getenv("GOOGLE_APPLICATION_CREDENTIALS");
  if (override_value != nullptr) {
    return override_value;
  }
  // There are probably more efficient ways to do this, but meh, the strings
  // are typically short, and this does not happen that often.
  auto root = std::getenv(GoogleApplicationDefaultCredentialsHomeVariable());
  if (root == nullptr) {
    std::ostringstream os;
    os << "The " << GoogleApplicationDefaultCredentialsHomeVariable()
       << " environment variable is not set. Cannot determine the default"
       << " path for service account credentials.";
    google::cloud::internal::RaiseRuntimeError(os.str());
  }
  return root + GoogleCredentialsSuffix();
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
