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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace v2 = ::google::storage::v2;

TEST(GrpcPredefinedAclParser, ToProtoObjectPredefinedAcl) {
  EXPECT_EQ(v2::OBJECT_ACL_AUTHENTICATED_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(v2::OBJECT_ACL_PRIVATE,
            GrpcPredefinedAclParser::ToProtoObject(PredefinedAcl::Private()));
  EXPECT_EQ(
      v2::OBJECT_ACL_PROJECT_PRIVATE,
      GrpcPredefinedAclParser::ToProtoObject(PredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(v2::OBJECT_ACL_PUBLIC_READ, GrpcPredefinedAclParser::ToProtoObject(
                                            PredefinedAcl::PublicRead()));
  EXPECT_EQ(
      v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED,
      GrpcPredefinedAclParser::ToProtoObject(PredefinedAcl::PublicReadWrite()));
  EXPECT_EQ(google::storage::v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedAcl::BucketOwnerFullControl()));
  EXPECT_EQ(
      google::storage::v2::OBJECT_ACL_BUCKET_OWNER_READ,
      GrpcPredefinedAclParser::ToProtoObject(PredefinedAcl::BucketOwnerRead()));
}

TEST(GrpcPredefinedAclParser, ToProtoObjectDestinationPredefinedAcl) {
  EXPECT_EQ(v2::OBJECT_ACL_AUTHENTICATED_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                DestinationPredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(v2::OBJECT_ACL_PRIVATE, GrpcPredefinedAclParser::ToProtoObject(
                                        DestinationPredefinedAcl::Private()));
  EXPECT_EQ(v2::OBJECT_ACL_PROJECT_PRIVATE,
            GrpcPredefinedAclParser::ToProtoObject(
                DestinationPredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(v2::OBJECT_ACL_PUBLIC_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                DestinationPredefinedAcl::PublicRead()));
  EXPECT_EQ(google::storage::v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL,
            GrpcPredefinedAclParser::ToProtoObject(
                DestinationPredefinedAcl::BucketOwnerFullControl()));
  EXPECT_EQ(google::storage::v2::OBJECT_ACL_BUCKET_OWNER_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                DestinationPredefinedAcl::BucketOwnerRead()));
}

TEST(GrpcPredefinedAclParser, ToProtoObjectDefaultObjectPredefinedAcl) {
  EXPECT_EQ(v2::OBJECT_ACL_AUTHENTICATED_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedDefaultObjectAcl::AuthenticatedRead()));
  EXPECT_EQ(v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedDefaultObjectAcl::BucketOwnerFullControl()));
  EXPECT_EQ(v2::OBJECT_ACL_BUCKET_OWNER_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedDefaultObjectAcl::BucketOwnerRead()));
  EXPECT_EQ(v2::OBJECT_ACL_PRIVATE, GrpcPredefinedAclParser::ToProtoObject(
                                        PredefinedDefaultObjectAcl::Private()));
  EXPECT_EQ(v2::OBJECT_ACL_PROJECT_PRIVATE,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedDefaultObjectAcl::ProjectPrivate()));
  EXPECT_EQ(v2::OBJECT_ACL_PUBLIC_READ,
            GrpcPredefinedAclParser::ToProtoObject(
                PredefinedDefaultObjectAcl::PublicRead()));
}

TEST(GrpcPredefinedAclParser, ToProtoBucket) {
  EXPECT_EQ(v2::BUCKET_ACL_AUTHENTICATED_READ,
            GrpcPredefinedAclParser::ToProtoBucket(
                PredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(v2::BUCKET_ACL_PRIVATE,
            GrpcPredefinedAclParser::ToProtoBucket(PredefinedAcl::Private()));
  EXPECT_EQ(
      v2::BUCKET_ACL_PROJECT_PRIVATE,
      GrpcPredefinedAclParser::ToProtoBucket(PredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(v2::BUCKET_ACL_PUBLIC_READ, GrpcPredefinedAclParser::ToProtoBucket(
                                            PredefinedAcl::PublicRead()));
  EXPECT_EQ(
      v2::BUCKET_ACL_PUBLIC_READ_WRITE,
      GrpcPredefinedAclParser::ToProtoBucket(PredefinedAcl::PublicReadWrite()));
  EXPECT_EQ(v2::PREDEFINED_BUCKET_ACL_UNSPECIFIED,
            GrpcPredefinedAclParser::ToProtoBucket(
                PredefinedAcl::BucketOwnerFullControl()));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
