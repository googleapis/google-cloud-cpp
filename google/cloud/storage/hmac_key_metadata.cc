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

#include "google/cloud/storage/hmac_key_metadata.h"
#include "google/cloud/storage/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::ostream& operator<<(std::ostream& os, HmacKeyMetadata const& rhs) {
  return os << "HmacKeyMetadata={id=" << rhs.id() << ", kind=" << rhs.kind()
            << ", access_id=" << rhs.access_id() << ", etag=" << rhs.etag()
            << ", project_id=" << rhs.project_id()
            << ", service_account_email=" << rhs.service_account_email()
            << ", state=" << rhs.state()
            << ", time_created=" << internal::FormatRfc3339(rhs.time_created())
            << ", updated=" << internal::FormatRfc3339(rhs.updated()) << "}";
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
