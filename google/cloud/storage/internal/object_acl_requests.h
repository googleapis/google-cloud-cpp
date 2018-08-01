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
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Generic ObjectAccessControl change.
 */
template <typename Derived>
class GenericChangeObjectAclRequest
    : public GenericObjectRequest<Derived, Generation, UserProject> {
 public:
  GenericChangeObjectAclRequest() = default;

  explicit GenericChangeObjectAclRequest(std::string bucket, std::string object,
                                         std::string entity, std::string role)
      : GenericObjectRequest<Derived, Generation, UserProject>(
            std::move(bucket), std::move(object)),
        entity_(std::move(entity)),
        role_(std::move(role)) {}

  std::string const& entity() const { return entity_; }
  GenericChangeObjectAclRequest& set_entity(std::string v) {
    entity_ = std::move(v);
    return *this;
  }
  std::string const& role() const { return role_; }
  GenericChangeObjectAclRequest& set_role(std::string v) {
    role_ = std::move(v);
    return *this;
  }

 private:
  std::string entity_;
  std::string role_;
};

/**
 * Create an ObjectAccessControl entry.
 */
class CreateObjectAclRequest
    : public GenericChangeObjectAclRequest<CreateObjectAclRequest> {
 public:
  using GenericChangeObjectAclRequest::GenericChangeObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, CreateObjectAclRequest const& r);

/**
 * Update an ObjectAccessControl entry.
 */
class UpdateObjectAclRequest
    : public GenericChangeObjectAclRequest<UpdateObjectAclRequest> {
 public:
  using GenericChangeObjectAclRequest::GenericChangeObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, UpdateObjectAclRequest const& r);

/// A request type for `ObjectAccessControl: {get,delete,patch,update}`
class ObjectAclRequest
    : public GenericObjectRequest<ObjectAclRequest, Generation, UserProject> {
 public:
  ObjectAclRequest() : GenericObjectRequest() {}
  ObjectAclRequest(std::string bucket, std::string object, std::string entity)
      : GenericObjectRequest(std::move(bucket), std::move(object)),
        entity_(std::move(entity)) {}

  std::string const& entity() const { return entity_; }
  ObjectAclRequest& set_entity(std::string v) {
    entity_ = std::move(v);
    return *this;
  }

 private:
  std::string entity_;
};

std::ostream& operator<<(std::ostream& os, ObjectAclRequest const& r);

/**
 * Patch an ObjectAccessControl entry.
 */
class PatchObjectAclRequest
    : public GenericObjectRequest<PatchObjectAclRequest, Generation,
                                  UserProject> {
 public:
  PatchObjectAclRequest(std::string bucket, std::string object,
                        std::string entity, ObjectAccessControl const& original,
                        ObjectAccessControl const& new_acl);
  PatchObjectAclRequest(std::string bucket, std::string object,
                        std::string entity,
                        ObjectAccessControlPatchBuilder const& patch);

  std::string const& entity() const { return entity_; }
  std::string const& payload() const { return payload_; }

 private:
  std::string entity_;
  std::string payload_;
};

std::ostream& operator<<(std::ostream& os, PatchObjectAclRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_ACL_REQUESTS_H_
