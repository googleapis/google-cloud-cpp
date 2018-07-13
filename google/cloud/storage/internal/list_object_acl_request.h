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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LIST_OBJECT_ACL_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LIST_OBJECT_ACL_REQUEST_H_

#include "google/cloud/storage/internal/generic_object_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Delete an object.
 */
class ListObjectAclRequest
    : public GenericObjectRequest<ListObjectAclRequest, Generation,
                                  UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, ListObjectAclRequest const& r);

/// Represent a response to the ListObjectAclRequest.
struct ListObjectAclResponse {
  static ListObjectAclResponse FromHttpResponse(HttpResponse&& response);

  std::vector<ObjectAccessControl> items;
};

std::ostream& operator<<(std::ostream& os, ListObjectAclResponse const& r);
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LIST_OBJECT_ACL_REQUEST_H_
