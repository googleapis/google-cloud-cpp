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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SERVICE_ACCOUNT_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SERVICE_ACCOUNT_REQUESTS_H_

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Requests the Google Cloud Storage service account for a project.
 */
class GetProjectServiceAccountRequest
    : public GenericRequest<GetProjectServiceAccountRequest, UserProject> {
 public:
  GetProjectServiceAccountRequest() = default;
  explicit GetProjectServiceAccountRequest(std::string project_id)
      : project_id_(std::move(project_id)) {}

  std::string const& project_id() const { return project_id_; }

 private:
  std::string project_id_;
};

std::ostream& operator<<(std::ostream& os,
                         GetProjectServiceAccountRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SERVICE_ACCOUNT_REQUESTS_H_
