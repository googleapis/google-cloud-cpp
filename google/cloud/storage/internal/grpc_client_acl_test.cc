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

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;

auto constexpr kBucketProtoText = R"pb(
  name: "projects/_/buckets/test-bucket-id"
  bucket_id: "test-bucket-id"
  project: "projects/123456"
  metageneration: 1234567
  location: "test-location"
  location_type: "REGIONAL"
  storage_class: "test-storage-class"
  rpo: "test-rpo"
  acl: { role: "test-role1" entity: "test-entity1" entity_alt: "test-alt1" }
  acl: { role: "test-role2" entity: "test-entity2" entity_alt: "test-alt2" }
  default_object_acl: {
    role: "test-role3"
    entity: "test-entity3"
    entity_alt: "test-alt3"
  }
  default_object_acl: {
    role: "test-role4"
    entity: "test-entity4"
    entity_alt: "test-alt4"
  }
  lifecycle {
    rule {
      action { type: "Delete" }
      condition {
        age_days: 90
        is_live: false
        matches_storage_class: "NEARLINE"
      }
    }
    rule {
      action { type: "SetStorageClass" storage_class: "NEARLINE" }
      condition { age_days: 7 is_live: true matches_storage_class: "STANDARD" }
    }
  }
  create_time: { seconds: 1565194924 nanos: 123456000 }
  cors: {
    origin: "test-origin-0"
    origin: "test-origin-1"
    method: "GET"
    method: "PUT"
    response_header: "test-header-0"
    response_header: "test-header-1"
    max_age_seconds: 1800
  }
  cors: {
    origin: "test-origin-2"
    origin: "test-origin-3"
    method: "POST"
    response_header: "test-header-3"
    max_age_seconds: 3600
  }
  update_time: { seconds: 1565194925 nanos: 123456000 }
  default_event_based_hold: true
  labels: { key: "test-key-1" value: "test-value-1" }
  labels: { key: "test-key-2" value: "test-value-2" }
  website { main_page_suffix: "index.html" not_found_page: "404.html" }
  versioning { enabled: true }
  logging {
    log_bucket: "test-log-bucket"
    log_object_prefix: "test-log-object-prefix"
  }
  owner { entity: "test-entity" entity_id: "test-entity-id" }
  encryption { default_kms_key: "test-default-kms-key-name" }
  billing { requester_pays: true }
  retention_policy {
    effective_time { seconds: 1565194926 nanos: 123456000 }
    is_locked: true
    retention_period: 86400
  }
  iam_config {
    uniform_bucket_level_access {
      enabled: true
      lock_time { seconds: 1565194927 nanos: 123456000 }
    }
    public_access_prevention: "inherited"
  }
)pb";

auto constexpr kObjectProtoText = R"pb(
  name: "test-object-id"
  bucket: "test-bucket-id"
  acl: { role: "test-role1" entity: "test-entity1" entity_alt: "test-alt1" }
  acl: { role: "test-role2" entity: "test-entity2" entity_alt: "test-alt2" }
  content_encoding: "test-content-encoding"
  content_disposition: "test-content-disposition"
  cache_control: "test-cache-control"
  content_language: "test-content-language"
  metageneration: 42
  delete_time: { seconds: 1565194924 nanos: 123456789 }
  content_type: "test-content-type"
  size: 123456
  create_time: { seconds: 1565194924 nanos: 234567890 }
  # These magic numbers can be obtained using `gsutil hash` and then
  # transforming the output from base64 to binary using tools like xxd(1).
  checksums {
    crc32c: 576848900
    md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
  }
  component_count: 7
  update_time: { seconds: 1565194924 nanos: 345678901 }
  storage_class: "test-storage-class"
  kms_key: "test-kms-key-name"
  update_storage_class_time: { seconds: 1565194924 nanos: 456789012 }
  temporary_hold: true
  retention_expire_time: { seconds: 1565194924 nanos: 567890123 }
  metadata: { key: "test-key-1" value: "test-value-1" }
  metadata: { key: "test-key-2" value: "test-value-2" }
  event_based_hold: true
  generation: 2345
  owner: { entity: "test-entity" entity_id: "test-entity-id" }
  customer_encryption: {
    encryption_algorithm: "test-encryption-algorithm"
    key_sha256_bytes: "01234567"
  }
)pb";

class GrpcClientAclTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

google::cloud::Options TestOptions() {
  return Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
}

std::shared_ptr<GrpcClient> CreateTestClient(
    std::shared_ptr<storage_internal::StorageStub> stub) {
  return GrpcClient::CreateMock(std::move(stub), TestOptions());
}

TEST_F(GrpcClientAclTest, ListBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListBucketAcl(
      ListBucketAclRequest("test-bucket-name")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, ListBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListBucketAcl(ListBucketAclRequest("test-bucket-id"));
  ASSERT_STATUS_OK(response);
  auto make_matcher = [](std::string const& role, std::string const& entity) {
    auto get_role = [](BucketAccessControl const& acl) { return acl.role(); };
    auto get_entity = [](BucketAccessControl const& acl) {
      return acl.entity();
    };
    auto get_bucket = [](BucketAccessControl const& acl) {
      return acl.bucket();
    };
    return AllOf(ResultOf(get_role, role), ResultOf(get_entity, entity),
                 ResultOf(get_bucket, "test-bucket-id"));
  };
  EXPECT_THAT(response->items,
              UnorderedElementsAre(make_matcher("test-role1", "test-entity1"),
                                   make_matcher("test-role2", "test-entity2")));
}

TEST_F(GrpcClientAclTest, GetBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetBucketAcl(
      GetBucketAclRequest("test-bucket-id", "test-entity1"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "test-role1");
  EXPECT_EQ(response->bucket(), "test-bucket-id");

  response =
      client->GetBucketAcl(GetBucketAclRequest("test-bucket-id", "test-alt1"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "test-role1");
  EXPECT_EQ(response->bucket(), "test-bucket-id");
}

TEST_F(GrpcClientAclTest, GetBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetBucketAcl(
      GetBucketAclRequest("test-bucket-name", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, GetBucketAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetBucketAcl(
      GetBucketAclRequest("test-bucket-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientAclTest, CreateBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));

        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-new-entity");
        expected.set_role("test-new-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));

        *response.mutable_acl() = request.bucket().acl();
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateBucketAcl(CreateBucketAclRequest(
      "test-bucket-id", "test-new-entity", "test-new-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-new-entity");
}

TEST_F(GrpcClientAclTest, CreateBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        // The `Field()` option is ignored, as the implementation only works
        // correctly if key fields are present.
        EXPECT_THAT(metadata, UnorderedElementsAre(Pair("x-goog-quota-user",
                                                        "test-quota-user")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateBucketAcl(
      CreateBucketAclRequest("test-bucket-name", "test-entity1", "test-role1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, CreateBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-new-entity");
        expected.set_role("test-new-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateBucketAcl(CreateBucketAclRequest(
      "test-bucket-id", "test-new-entity", "test-new-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, DeleteBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(2)
      .WillRepeatedly([](grpc::ClientContext&,
                         v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));

        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        *response.mutable_acl() = request.bucket().acl();
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-id", "test-entity1"));
  EXPECT_STATUS_OK(response);

  response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-id", "test-alt1"));
  EXPECT_STATUS_OK(response);
}

TEST_F(GrpcClientAclTest, DeleteBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        // The `Field()` option is ignored, as the implementation only works
        // correctly if key fields are present.
        EXPECT_THAT(metadata, UnorderedElementsAre(Pair("x-goog-quota-user",
                                                        "test-quota-user")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-name", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, DeleteBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-entity2");
        expected.set_role("test-role2");
        EXPECT_THAT(request.bucket().acl(),
                    ElementsAre(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-id", "test-entity1"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, DeleteBucketAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientAclTest, UpdateBucketSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(2)
      .WillRepeatedly([](grpc::ClientContext&,
                         v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        for (auto& a : *response.mutable_acl()) {
          if (a.entity() == "test-entity1") a.set_role("updated-role");
        }
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucketAcl(
      UpdateBucketAclRequest("test-bucket-id", "test-entity1", "updated-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");

  response = client->UpdateBucketAcl(
      UpdateBucketAclRequest("test-bucket-id", "test-alt1", "updated-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");
}

TEST_F(GrpcClientAclTest, UpdateBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        // The `Field()` option is ignored, as the implementation only works
        // correctly if key fields are present.
        EXPECT_THAT(metadata, UnorderedElementsAre(Pair("x-goog-quota-user",
                                                        "test-quota-user")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucketAcl(
      UpdateBucketAclRequest("test-bucket-name", "test-entity1", "updated-role")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, UpdateBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-entity1");
        expected.set_role("updated-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucketAcl(
      UpdateBucketAclRequest("test-bucket-id", "test-entity1", "updated-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, PatchBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(2)
      .WillRepeatedly([](grpc::ClientContext&,
                         v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        for (auto& a : *response.mutable_acl()) {
          if (a.entity() == "test-entity1") a.set_role("updated-role");
        }
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchBucketAcl(PatchBucketAclRequest(
      "test-bucket-id", "test-entity1",
      BucketAccessControlPatchBuilder().set_role("updated-role")));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");

  response = client->PatchBucketAcl(PatchBucketAclRequest(
      "test-bucket-id", "test-alt1",
      BucketAccessControlPatchBuilder().set_role("updated-role")));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");
}

TEST_F(GrpcClientAclTest, PatchBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchBucketAcl(
      PatchBucketAclRequest(
          "test-bucket-name", "test-entity1",
          BucketAccessControlPatchBuilder().set_role("updated-role"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, PatchBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-entity1");
        expected.set_role("updated-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchBucketAcl(PatchBucketAclRequest(
      "test-bucket-id", "test-entity1",
      BucketAccessControlPatchBuilder().set_role("updated-role")));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, ListObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.object(), "test-object-id");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListObjectAcl(
      ListObjectAclRequest("test-bucket-name", "test-object-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, ListObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListObjectAcl(
      ListObjectAclRequest("test-bucket-id", "test-object-id"));
  ASSERT_STATUS_OK(response);
  auto make_matcher = [](std::string const& role, std::string const& entity) {
    auto get_role = [](ObjectAccessControl const& acl) { return acl.role(); };
    auto get_entity = [](ObjectAccessControl const& acl) {
      return acl.entity();
    };
    auto get_bucket = [](ObjectAccessControl const& acl) {
      return acl.bucket();
    };
    auto get_object = [](ObjectAccessControl const& acl) {
      return acl.object();
    };
    return AllOf(ResultOf(get_role, role), ResultOf(get_entity, entity),
                 ResultOf(get_bucket, "test-bucket-id"),
                 ResultOf(get_object, "test-object-id"));
  };
  EXPECT_THAT(response->items,
              UnorderedElementsAre(make_matcher("test-role1", "test-entity1"),
                                   make_matcher("test-role2", "test-entity2")));
}

TEST_F(GrpcClientAclTest, GetObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetObjectAcl(
      GetObjectAclRequest("test-bucket-id", "test-object-id", "test-entity1"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "test-role1");
  EXPECT_EQ(response->object(), "test-object-id");

  response = client->GetObjectAcl(
      GetObjectAclRequest("test-bucket-id", "test-object-id", "test-alt1"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "test-role1");
  EXPECT_EQ(response->object(), "test-object-id");
}

TEST_F(GrpcClientAclTest, GetObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket-id");
        EXPECT_THAT(request.object(), "test-object-id");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetObjectAcl(
      GetObjectAclRequest("test-bucket-id", "test-object-id", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, GetObjectAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetObjectAcl(GetObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientAclTest, CreateObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateObjectRequest const& request) {
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));

        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-new-entity");
        expected.set_role("test-new-role");
        EXPECT_THAT(request.object().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));

        *response.mutable_acl() = request.object().acl();
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateObjectAcl(CreateObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-new-entity", "test-new-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-new-entity");
}

TEST_F(GrpcClientAclTest, CreateObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.object(), "test-object-id");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateObjectAcl(
      CreateObjectAclRequest("test-bucket-name", "test-object-id",
                             "test-entity1", "test-role1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, CreateObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateObjectRequest const& request) {
        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-new-entity");
        expected.set_role("test-new-role");
        EXPECT_THAT(request.object().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateObjectAcl(CreateObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-new-entity", "test-new-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, DeleteObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::UpdateObjectRequest const& request) {
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        *response.mutable_acl() = request.object().acl();
        response.set_metageneration(response.metageneration());
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteObjectAcl(DeleteObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-entity1"));
  EXPECT_STATUS_OK(response);

  response = client->DeleteObjectAcl(
      DeleteObjectAclRequest("test-bucket-id", "test-object-id", "test-alt2"));
  EXPECT_STATUS_OK(response);
}

TEST_F(GrpcClientAclTest, DeleteObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket-id");
        EXPECT_THAT(request.object(), "test-object-id");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteObjectAcl(
      DeleteObjectAclRequest("test-bucket-id", "test-object-id", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, DeleteObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce(
          [](grpc::ClientContext&, v2::UpdateObjectRequest const& request) {
            EXPECT_EQ(request.object().bucket(),
                      "projects/_/buckets/test-bucket-id");
            EXPECT_EQ(request.object().name(), "test-object-id");
            auto expected = v2::ObjectAccessControl();
            expected.set_entity("test-entity2");
            expected.set_role("test-role2");
            EXPECT_THAT(request.object().acl(),
                        ElementsAre(IsProtoEqual(expected)));
            EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
            return Status(StatusCode::kFailedPrecondition, "conflict");
          });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteObjectAcl(DeleteObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-entity1"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, DeleteObjectAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->DeleteObjectAcl(DeleteObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientAclTest, UpdateObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::UpdateObjectRequest const& request) {
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        for (auto& a : *response.mutable_acl()) {
          if (a.entity() == "test-entity1") a.set_role("updated-role");
        }
        response.set_metageneration(response.metageneration());
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateObjectAcl(UpdateObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-entity1", "updated-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");

  response = client->UpdateObjectAcl(UpdateObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-alt1", "updated-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");
}

TEST_F(GrpcClientAclTest, UpdateObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket-id");
        EXPECT_THAT(request.object(), "test-object-id");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateObjectAcl(
      UpdateObjectAclRequest("test-bucket-id", "test-object-id", "test-entity1",
                             "updated-role")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, UpdateObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateObjectRequest const& request) {
        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-entity1");
        expected.set_role("updated-role");
        EXPECT_THAT(request.object().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateObjectAcl(UpdateObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-entity1", "updated-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, PatchObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::UpdateObjectRequest const& request) {
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        for (auto& a : *response.mutable_acl()) {
          if (a.entity() == "test-entity1") a.set_role("updated-role");
        }
        response.set_metageneration(response.metageneration());
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchObjectAcl(PatchObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-entity1",
      ObjectAccessControlPatchBuilder().set_role("updated-role")));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");

  response = client->PatchObjectAcl(PatchObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-alt1",
      ObjectAccessControlPatchBuilder().set_role("updated-role")));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "updated-role");
}

TEST_F(GrpcClientAclTest, PatchObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket-id");
        EXPECT_THAT(request.object(), "test-object-id");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchObjectAcl(
      PatchObjectAclRequest(
          "test-bucket-id", "test-object-id", "test-entity1",
          ObjectAccessControlPatchBuilder().set_role("updated-role"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, PatchObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([&](grpc::ClientContext&, v2::GetObjectRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Object response;
        EXPECT_TRUE(TextFormat::ParseFromString(kObjectProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateObjectRequest const& request) {
        EXPECT_EQ(request.object().bucket(),
                  "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.object().name(), "test-object-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-entity1");
        expected.set_role("updated-role");
        EXPECT_THAT(request.object().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchObjectAcl(PatchObjectAclRequest(
      "test-bucket-id", "test-object-id", "test-entity1",
      ObjectAccessControlPatchBuilder().set_role("updated-role")));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, ListDefaultObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListDefaultObjectAcl(
      ListDefaultObjectAclRequest("test-bucket-name")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, ListDefaultObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListDefaultObjectAcl(
      ListDefaultObjectAclRequest("test-bucket-id"));
  ASSERT_STATUS_OK(response);
  auto make_matcher = [](std::string const& role, std::string const& entity) {
    auto get_role = [](ObjectAccessControl const& acl) { return acl.role(); };
    auto get_entity = [](ObjectAccessControl const& acl) {
      return acl.entity();
    };
    auto get_bucket = [](ObjectAccessControl const& acl) {
      return acl.bucket();
    };
    return AllOf(ResultOf(get_role, role), ResultOf(get_entity, entity),
                 ResultOf(get_bucket, "test-bucket-id"));
  };
  EXPECT_THAT(response->items,
              UnorderedElementsAre(make_matcher("test-role3", "test-entity3"),
                                   make_matcher("test-role4", "test-entity4")));
}

TEST_F(GrpcClientAclTest, GetDefaultObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetDefaultObjectAcl(
      GetDefaultObjectAclRequest("test-bucket-id", "test-entity3"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity3");
  EXPECT_EQ(response->role(), "test-role3");
  EXPECT_EQ(response->bucket(), "test-bucket-id");

  response = client->GetDefaultObjectAcl(
      GetDefaultObjectAclRequest("test-bucket-id", "test-alt3"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity3");
  EXPECT_EQ(response->role(), "test-role3");
  EXPECT_EQ(response->bucket(), "test-bucket-id");
}

TEST_F(GrpcClientAclTest, GetDefaultObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetDefaultObjectAcl(
      GetDefaultObjectAclRequest("test-bucket-name", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, GetDefaultObjectAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetDefaultObjectAcl(
      GetDefaultObjectAclRequest("test-bucket-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientAclTest, CreateDefaultObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        *response.mutable_default_object_acl() =
            request.bucket().default_object_acl();
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateDefaultObjectAcl(CreateDefaultObjectAclRequest(
      "test-bucket-id", "test-new-entity", "test-new-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-new-entity");
  EXPECT_EQ(response->role(), "test-new-role");
}

TEST_F(GrpcClientAclTest, CreateDefaultObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest("test-bucket-name", "test-entity3",
                                    "test-role3")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, CreateDefaultObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-new-entity");
        expected.set_role("test-new-role");
        EXPECT_THAT(request.bucket().default_object_acl(),
                    Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateDefaultObjectAcl(CreateDefaultObjectAclRequest(
      "test-bucket-id", "test-new-entity", "test-new-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, DeleteDefaultObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(2)
      .WillRepeatedly([](grpc::ClientContext&,
                         v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        *response.mutable_default_object_acl() =
            request.bucket().default_object_acl();
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("test-bucket-id", "test-entity3"));
  EXPECT_STATUS_OK(response);
  response = client->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("test-bucket-id", "test-alt3"));
  EXPECT_STATUS_OK(response);
}

TEST_F(GrpcClientAclTest, DeleteDefaultObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("test-bucket-name", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, DeleteDefaultObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-entity4");
        expected.set_role("test-role4");
        EXPECT_THAT(request.bucket().default_object_acl(),
                    ElementsAre(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("test-bucket-id", "test-entity3"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, DeleteDefaultObjectAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("test-bucket-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientAclTest, UpdateDefaultObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(2)
      .WillRepeatedly([](grpc::ClientContext&,
                         v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        for (auto& a : *response.mutable_default_object_acl()) {
          if (a.entity() == "test-entity3") a.set_role("updated-role");
        }
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateDefaultObjectAcl(UpdateDefaultObjectAclRequest(
      "test-bucket-id", "test-entity3", "updated-role"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity3");
  EXPECT_EQ(response->role(), "updated-role");

  response = client->UpdateDefaultObjectAcl(UpdateDefaultObjectAclRequest(
      "test-bucket-id", "test-alt3", "updated-role"));
  ASSERT_STATUS_OK(response);
}

TEST_F(GrpcClientAclTest, UpdateDefaultObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest("test-bucket-name", "test-entity3",
                                    "updated-role")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, UpdateDefaultObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-entity3");
        expected.set_role("updated-role");
        EXPECT_THAT(request.bucket().default_object_acl(),
                    Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateDefaultObjectAcl(UpdateDefaultObjectAclRequest(
      "test-bucket-id", "test-entity3", "updated-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientAclTest, PatchDefaultObjectAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .Times(2)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(2)
      .WillRepeatedly([](grpc::ClientContext&,
                         v2::UpdateBucketRequest const& request) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        EXPECT_EQ(request.if_metageneration_match(), response.metageneration());
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        for (auto& a : *response.mutable_default_object_acl()) {
          if (a.entity() == "test-entity3") a.set_role("updated-role");
        }
        response.set_metageneration(response.metageneration() + 1);
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
      "test-bucket-id", "test-entity3",
      ObjectAccessControlPatchBuilder().set_role("updated-role")));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity3");
  EXPECT_EQ(response->role(), "updated-role");

  response = client->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
      "test-bucket-id", "test-alt3",
      ObjectAccessControlPatchBuilder().set_role("updated-role")));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity3");
  EXPECT_EQ(response->role(), "updated-role");
}

TEST_F(GrpcClientAclTest, PatchDefaultObjectAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchDefaultObjectAcl(
      PatchDefaultObjectAclRequest(
          "test-bucket-name", "test-entity3",
          ObjectAccessControlPatchBuilder().set_role("updated-role"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientAclTest, PatchDefaultObjectAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const& request) {
        EXPECT_TRUE(request.has_read_mask());
        EXPECT_THAT(request.read_mask().paths(), ElementsAre("*"));
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::ObjectAccessControl();
        expected.set_entity("test-entity3");
        expected.set_role("updated-role");
        EXPECT_THAT(request.bucket().default_object_acl(),
                    Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(),
                    ElementsAre("default_object_acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
      "test-bucket-id", "test-entity3",
      ObjectAccessControlPatchBuilder().set_role("updated-role")));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
