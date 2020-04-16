// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_VERSION_H

#include "google/cloud/bigquery/version_info.h"
#include "google/cloud/version.h"
#include <sstream>
#include <string>

#define BIGQUERY_CLIENT_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(BIGQUERY_CLIENT_VERSION_MAJOR, \
                         BIGQUERY_CLIENT_VERSION_MINOR)

namespace google {
namespace cloud {
namespace bigquery {

// The inlined, versioned namespace for the client APIs.
//
// Applications may need to link multiple versions of the C++ client,
// for example, if they link a library that uses an older version of
// the client than they do.  This namespace is inlined, so
// applications can use `bigquery::Foo` in their source, but the
// symbols are versioned, e.g., the symbol becomes
// `bigquery::v1::Foo`.
inline namespace BIGQUERY_CLIENT_NS {
int constexpr VersionMajor() { return BIGQUERY_CLIENT_VERSION_MAJOR; }
int constexpr VersionMinor() { return BIGQUERY_CLIENT_VERSION_MINOR; }
int constexpr VersionPatch() { return BIGQUERY_CLIENT_VERSION_PATCH; }

// Returns a single integer representing the major, minor, and patch
// version.
int constexpr Version() {
  return 100 * (100 * VersionMajor() + VersionMinor()) + VersionPatch();
}

// Returns the version as a string in the form `MAJOR.MINOR.PATCH`.
std::string VersionString();

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_VERSION_H
