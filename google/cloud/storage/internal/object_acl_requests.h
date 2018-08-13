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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_ACL_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_ACL_REQUESTS_H_

#include "google/cloud/storage/internal/generic_object_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Represents a request for the `ObjectAccessControls: list` API.
 */
class ListObjectAclRequest
    : public GenericObjectRequest<ListObjectAclRequest, Generation,
                                  UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, ListObjectAclRequest const& r);

/// Represents a response to the `ObjectAccessControls: list` API.
struct ListObjectAclResponse {
  static ListObjectAclResponse FromHttpResponse(HttpResponse&& response);

  std::vector<ObjectAccessControl> items;
};

std::ostream& operator<<(std::ostream& os, ListObjectAclResponse const& r);

/**
 * Represents common attributes to multiple `ObjectAccessControls` request
 * types.
 *
 * The classes that represent requests for the `ObjectAccessControls: create`,
 * `get`, `delete`, `patch`, and `update` APIs have a lot of commonality. This
 * template class refactors that code.
 */
template <typename Derived>
class GenericObjectAclRequest
    : public GenericObjectRequest<Derived, Generation, UserProject> {
 public:
  GenericObjectAclRequest() = default;
  GenericObjectAclRequest(std::string bucket, std::string object,
                          std::string entity)
      : GenericObjectRequest<Derived, Generation, UserProject>(
            std::move(bucket), std::move(object)),
        entity_(std::move(entity)) {}

  std::string const& entity() const { return entity_; }

 private:
  std::string entity_;
};

/**
 * Represents a request to call the `ObjectAccessControls: get` API.
 */
class GetObjectAclRequest
    : public GenericObjectAclRequest<GetObjectAclRequest> {
  using GenericObjectAclRequest::GenericObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, GetObjectAclRequest const& r);

/**
 * Represents a request to call the `ObjectAccessControls: delete` API.
 */
class DeleteObjectAclRequest
    : public GenericObjectAclRequest<DeleteObjectAclRequest> {
  using GenericObjectAclRequest::GenericObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, DeleteObjectAclRequest const& r);

/**
 * Represents common attributes to multiple `ObjectAccessControls` request
 * types.
 *
 * The classes that represent requests for the `ObjectAccessControls: create`,
 * `patch`, and `update` APIs have a lot of commonality. This
 * template class refactors that code.
 */
template <typename Derived>
class GenericChangeObjectAclRequest : public GenericObjectAclRequest<Derived> {
 public:
  GenericChangeObjectAclRequest() = default;

  explicit GenericChangeObjectAclRequest(std::string bucket, std::string object,
                                         std::string entity, std::string role)
      : GenericObjectAclRequest<Derived>(std::move(bucket), std::move(object),
                                         std::move(entity)),
        role_(std::move(role)) {}

  std::string const& role() const { return role_; }

 private:
  std::string role_;
};

/**
 * Represents a request to call the `ObjectAccessControls: insert` API.
 */
class CreateObjectAclRequest
    : public GenericChangeObjectAclRequest<CreateObjectAclRequest> {
 public:
  using GenericChangeObjectAclRequest::GenericChangeObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, CreateObjectAclRequest const& r);

/**
 * Represents a request to call the `ObjectAccessControls: update` API.
 */
class UpdateObjectAclRequest
    : public GenericChangeObjectAclRequest<UpdateObjectAclRequest> {
 public:
  using GenericChangeObjectAclRequest::GenericChangeObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, UpdateObjectAclRequest const& r);

/**
 * Represents a request to call the `ObjectAccessControls: patch` API.
 */
class PatchObjectAclRequest
    : public GenericObjectAclRequest<PatchObjectAclRequest> {
 public:
  PatchObjectAclRequest(std::string bucket, std::string object,
                        std::string entity, ObjectAccessControl const& original,
                        ObjectAccessControl const& new_acl);
  PatchObjectAclRequest(std::string bucket, std::string object,
                        std::string entity,
                        ObjectAccessControlPatchBuilder const& patch);

  std::string const& payload() const { return payload_; }

 private:
  std::string payload_;
};

std::ostream& operator<<(std::ostream& os, PatchObjectAclRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_ACL_REQUESTS_H_
