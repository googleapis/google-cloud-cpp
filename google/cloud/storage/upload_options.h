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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_UPLOAD_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_UPLOAD_OPTIONS_H

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Request a resumable upload, restoring a previous session if necessary.
 *
 * When this option is used the client library prefers using resumable uploads.
 *
 * If the value passed to this option is the empty string, then the library will
 * create a new resumable session. Otherwise the value should be the id of a
 * previous upload sesion, the client library will restore that session in
 * this case.
 */
struct UseResumableUploadSession
    : public internal::ComplexOption<UseResumableUploadSession, std::string> {
  using ComplexOption<UseResumableUploadSession, std::string>::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  UseResumableUploadSession() = default;

  static char const* name() { return "resumable-upload"; }
};

/// Create a UseResumableUploadSession option that restores previous sessions.
inline UseResumableUploadSession RestoreResumableUploadSession(
    std::string session_id) {
  return UseResumableUploadSession(std::move(session_id));
}

/// Create a UseResumableUploadSession option that requests new sessions.
inline UseResumableUploadSession NewResumableUploadSession() {
  return UseResumableUploadSession("");
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_UPLOAD_OPTIONS_H
