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

#include "google/cloud/storage/internal/raw_client.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

StatusOr<std::unique_ptr<ResumableUploadSession>>
RawClient::RestoreResumableSession(std::string const& /*session_id*/) {
  return Status(StatusCode::kUnimplemented, "removed, see #7282 for details");
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
