// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_predefined_acl_parser.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

google::storage::v2::PredefinedObjectAcl ToProtoObjectAcl(
    std::string const& value) {
  if (value == PredefinedAcl::BucketOwnerFullControl().value()) {
    return google::storage::v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL;
  }
  if (value == PredefinedAcl::BucketOwnerRead().value()) {
    return google::storage::v2::OBJECT_ACL_BUCKET_OWNER_READ;
  }
  if (value == PredefinedAcl::AuthenticatedRead().value()) {
    return google::storage::v2::OBJECT_ACL_AUTHENTICATED_READ;
  }
  if (value == PredefinedAcl::Private().value()) {
    return google::storage::v2::OBJECT_ACL_PRIVATE;
  }
  if (value == PredefinedAcl::ProjectPrivate().value()) {
    return google::storage::v2::OBJECT_ACL_PROJECT_PRIVATE;
  }
  if (value == PredefinedAcl::PublicRead().value()) {
    return google::storage::v2::OBJECT_ACL_PUBLIC_READ;
  }
  if (value == PredefinedAcl::PublicReadWrite().value()) {
    GCP_LOG(ERROR) << "Invalid predefinedAcl value " << value;
    return google::storage::v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << value;
  return google::storage::v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
}

}  // namespace

google::storage::v2::PredefinedObjectAcl GrpcPredefinedAclParser::ToProtoObject(
    PredefinedAcl const& acl) {
  return ToProtoObjectAcl(acl.value());
}

google::storage::v2::PredefinedObjectAcl GrpcPredefinedAclParser::ToProtoObject(
    DestinationPredefinedAcl const& acl) {
  return ToProtoObjectAcl(acl.value());
}

google::storage::v2::PredefinedObjectAcl GrpcPredefinedAclParser::ToProtoObject(
    PredefinedDefaultObjectAcl const& acl) {
  return ToProtoObjectAcl(acl.value());
}

google::storage::v2::PredefinedBucketAcl GrpcPredefinedAclParser::ToProtoBucket(
    PredefinedAcl const& acl) {
  auto const& value = acl.value();
  if (value == PredefinedAcl::AuthenticatedRead().value()) {
    return google::storage::v2::BUCKET_ACL_AUTHENTICATED_READ;
  }
  if (value == PredefinedAcl::Private().value()) {
    return google::storage::v2::BUCKET_ACL_PRIVATE;
  }
  if (value == PredefinedAcl::ProjectPrivate().value()) {
    return google::storage::v2::BUCKET_ACL_PROJECT_PRIVATE;
  }
  if (value == PredefinedAcl::PublicRead().value()) {
    return google::storage::v2::BUCKET_ACL_PUBLIC_READ;
  }
  if (value == PredefinedAcl::PublicReadWrite().value()) {
    return google::storage::v2::BUCKET_ACL_PUBLIC_READ_WRITE;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << value;
  return google::storage::v2::PREDEFINED_BUCKET_ACL_UNSPECIFIED;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
