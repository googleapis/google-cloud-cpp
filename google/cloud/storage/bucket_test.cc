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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
// TODO(#5929) - remove once deprecated functions are removed
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::ReturnRef;

/**
 * Test the functions in Storage::Client related to 'Buckets: *'.
 *
 * In general, this file should include for the APIs listed in:
 *
 * https://cloud.google.com/storage/docs/json_api/v1/buckets
 */
class BucketTest : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(BucketTest, CreateBucket) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "test-bucket-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name",
      "projectNumber": "123456789",
      "name": "test-bucket-name",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": 7,
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  auto expected = internal::BucketMetadataParser::FromString(text).value();

  ClientOptions mock_options(oauth2::CreateAnonymousCredentials());
  mock_options.set_project_id("test-project-name");

  EXPECT_CALL(*mock_, client_options()).WillRepeatedly(ReturnRef(mock_options));
  EXPECT_CALL(*mock_, CreateBucket)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce([&expected](internal::CreateBucketRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.metadata().name());
        EXPECT_EQ("US", r.metadata().location());
        EXPECT_EQ("STANDARD", r.metadata().storage_class());
        EXPECT_EQ("test-project-name", r.project_id());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.CreateBucket(
      "test-bucket-name",
      BucketMetadata().set_location("US").set_storage_class("STANDARD"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, CreateBucketTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketMetadata>(
      mock_, EXPECT_CALL(*mock_, CreateBucket),
      [](Client& client) {
        return client
            .CreateBucketForProject("test-bucket-name", "test-project-name",
                                    BucketMetadata())
            .status();
      },
      "CreateBucket");
}

TEST_F(BucketTest, CreateBucketPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<BucketMetadata>(
      client, EXPECT_CALL(*mock_, CreateBucket),
      [](Client& client) {
        return client
            .CreateBucketForProject("test-bucket-name", "test-project-name",
                                    BucketMetadata())
            .status();
      },
      "CreateBucket");
}

TEST_F(BucketTest, GetBucketMetadata) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar-baz",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "locationType": "regional",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  auto expected = internal::BucketMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce([&expected](internal::GetBucketMetadataRequest const& r) {
        EXPECT_EQ("foo-bar-baz", r.bucket_name());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.GetBucketMetadata("foo-bar-baz");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, GetMetadataTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketMetadata>(
      mock_, EXPECT_CALL(*mock_, GetBucketMetadata),
      [](Client& client) {
        return client.GetBucketMetadata("test-bucket-name").status();
      },
      "GetBucketMetadata");
}

TEST_F(BucketTest, GetMetadataPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<BucketMetadata>(
      client, EXPECT_CALL(*mock_, GetBucketMetadata),
      [](Client& client) {
        return client.GetBucketMetadata("test-bucket-name").status();
      },
      "GetBucketMetadata");
}

TEST_F(BucketTest, DeleteBucket) {
  EXPECT_CALL(*mock_, DeleteBucket)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteBucketRequest const& r) {
        EXPECT_EQ("foo-bar-baz", r.bucket_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteBucket("foo-bar-baz");
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketTest, DeleteBucketTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock_, EXPECT_CALL(*mock_, DeleteBucket),
      [](Client& client) { return client.DeleteBucket("test-bucket-name"); },
      [](Client& client) {
        return client.DeleteBucket("test-bucket-name",
                                   IfMetagenerationMatch(42));
      },
      "DeleteBucket");
}

TEST_F(BucketTest, DeleteBucketPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      client, EXPECT_CALL(*mock_, DeleteBucket),
      [](Client& client) { return client.DeleteBucket("test-bucket-name"); },
      "DeleteBucket");
}

TEST_F(BucketTest, UpdateBucket) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "test-bucket-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name",
      "projectNumber": "123456789",
      "name": "test-bucket-name",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": 7,
      "location": "US",
      "locationType": "regional",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  auto expected = internal::BucketMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, UpdateBucket)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce([&expected](internal::UpdateBucketRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.metadata().name());
        EXPECT_EQ("US", r.metadata().location());
        EXPECT_EQ("STANDARD", r.metadata().storage_class());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.UpdateBucket(
      "test-bucket-name",
      BucketMetadata().set_location("US").set_storage_class("STANDARD"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, UpdateBucketTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketMetadata>(
      mock_, EXPECT_CALL(*mock_, UpdateBucket),
      [](Client& client) {
        return client.UpdateBucket("test-bucket-name", BucketMetadata())
            .status();
      },
      [](Client& client) {
        return client
            .UpdateBucket("test-bucket-name", BucketMetadata(),
                          IfMetagenerationMatch(42))
            .status();
      },
      "UpdateBucket");
}

TEST_F(BucketTest, UpdateBucketPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<BucketMetadata>(
      client, EXPECT_CALL(*mock_, UpdateBucket),
      [](Client& client) {
        return client.UpdateBucket("test-bucket-name", BucketMetadata())
            .status();
      },
      "UpdateBucket");
}

TEST_F(BucketTest, PatchBucket) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "test-bucket-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name",
      "projectNumber": "123456789",
      "name": "test-bucket-name",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": 7,
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  auto expected = internal::BucketMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, PatchBucket)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce([&expected](internal::PatchBucketRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket());
        EXPECT_THAT(r.payload(), HasSubstr("STANDARD"));
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.PatchBucket(
      "test-bucket-name",
      BucketMetadataPatchBuilder().SetStorageClass("STANDARD"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, PatchBucketTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketMetadata>(
      mock_, EXPECT_CALL(*mock_, PatchBucket),
      [](Client& client) {
        return client
            .PatchBucket("test-bucket-name", BucketMetadataPatchBuilder())
            .status();
      },
      [](Client& client) {
        return client
            .PatchBucket("test-bucket-name", BucketMetadataPatchBuilder(),
                         IfMetagenerationMatch(42))
            .status();
      },
      "PatchBucket");
}

TEST_F(BucketTest, PatchBucketPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<BucketMetadata>(
      client, EXPECT_CALL(*mock_, PatchBucket),
      [](Client& client) {
        return client
            .PatchBucket("test-bucket-name", BucketMetadataPatchBuilder())
            .status();
      },
      "PatchBucket");
}

TEST_F(BucketTest, GetBucketIamPolicy) {
  IamBindings bindings;
  bindings.AddMember("roles/storage.admin", "test-user");
  IamPolicy expected{0, bindings, "XYZ="};

  EXPECT_CALL(*mock_, GetBucketIamPolicy)
      .WillOnce(Return(StatusOr<IamPolicy>(TransientError())))
      .WillOnce([&expected](internal::GetBucketIamPolicyRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.GetBucketIamPolicy("test-bucket-name");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, GetBucketIamPolicyTooManyFailures) {
  testing::TooManyFailuresStatusTest<IamPolicy>(
      mock_, EXPECT_CALL(*mock_, GetBucketIamPolicy),
      [](Client& client) {
        return client.GetBucketIamPolicy("test-bucket-name").status();
      },
      "GetBucketIamPolicy");
}

TEST_F(BucketTest, GetBucketIamPolicyPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<IamPolicy>(
      client, EXPECT_CALL(*mock_, GetBucketIamPolicy),
      [](Client& client) {
        return client.GetBucketIamPolicy("test-bucket-name").status();
      },
      "GetBucketIamPolicy");
}

TEST_F(BucketTest, SetBucketIamPolicy) {
  IamBindings bindings;
  bindings.AddMember("roles/storage.admin", "test-user");
  IamPolicy expected{0, bindings, "XYZ="};

  EXPECT_CALL(*mock_, SetBucketIamPolicy)
      .WillOnce(Return(StatusOr<IamPolicy>(TransientError())))
      .WillOnce([&expected](internal::SetBucketIamPolicyRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_THAT(r.json_payload(), HasSubstr("test-user"));
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.SetBucketIamPolicy("test-bucket-name", expected);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, SetBucketIamPolicyTooManyFailures) {
  testing::TooManyFailuresStatusTest<IamPolicy>(
      mock_, EXPECT_CALL(*mock_, SetBucketIamPolicy),
      [](Client& client) {
        return client.SetBucketIamPolicy("test-bucket-name", IamPolicy{})
            .status();
      },
      [](Client& client) {
        return client
            .SetBucketIamPolicy("test-bucket-name", IamPolicy{},
                                IfMatchEtag("ABC="))
            .status();
      },
      "SetBucketIamPolicy");
}

TEST_F(BucketTest, SetBucketIamPolicyPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<IamPolicy>(
      client, EXPECT_CALL(*mock_, SetBucketIamPolicy),
      [](Client& client) {
        return client.SetBucketIamPolicy("test-bucket-name", IamPolicy{})
            .status();
      },
      "SetBucketIamPolicy");
}

TEST_F(BucketTest, TestBucketIamPermissions) {
  internal::TestBucketIamPermissionsResponse expected;
  expected.permissions.emplace_back("storage.buckets.delete");

  EXPECT_CALL(*mock_, TestBucketIamPermissions)
      .WillOnce(Return(StatusOr<internal::TestBucketIamPermissionsResponse>(
          TransientError())))
      .WillOnce(
          [&expected](internal::TestBucketIamPermissionsRequest const& r) {
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_THAT(r.permissions(), ElementsAre("storage.buckets.delete"));
            return make_status_or(expected);
          });
  auto client = ClientForMock();
  auto actual = client.TestBucketIamPermissions("test-bucket-name",
                                                {"storage.buckets.delete"});
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, ElementsAreArray(expected.permissions));
}

TEST_F(BucketTest, TestBucketIamPermissionsTooManyFailures) {
  testing::TooManyFailuresStatusTest<
      internal::TestBucketIamPermissionsResponse>(
      mock_, EXPECT_CALL(*mock_, TestBucketIamPermissions),
      [](Client& client) {
        return client.TestBucketIamPermissions("test-bucket-name", {}).status();
      },
      "TestBucketIamPermissions");
}

TEST_F(BucketTest, TestBucketIamPermissionsPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<
      internal::TestBucketIamPermissionsResponse>(
      client, EXPECT_CALL(*mock_, TestBucketIamPermissions),
      [](Client& client) {
        return client.TestBucketIamPermissions("test-bucket-name", {}).status();
      },
      "TestBucketIamPermissions");
}

TEST_F(BucketTest, LockBucketRetentionPolicy) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "test-bucket-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name",
      "projectNumber": "123456789",
      "name": "test-bucket-name",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": 7,
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  auto expected = internal::BucketMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, LockBucketRetentionPolicy)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(
          [expected](internal::LockBucketRetentionPolicyRequest const& r) {
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_EQ(42, r.metageneration());
            return make_status_or(expected);
          });
  auto client = ClientForMock();
  auto metadata = client.LockBucketRetentionPolicy("test-bucket-name", 42U);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(expected, *metadata);
}

TEST_F(BucketTest, LockBucketRetentionPolicyTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketMetadata>(
      mock_, EXPECT_CALL(*mock_, LockBucketRetentionPolicy),
      [](Client& client) {
        return client.LockBucketRetentionPolicy("test-bucket-name", 1U)
            .status();
      },
      "LockBucketRetentionPolicy");
}

TEST_F(BucketTest, LockBucketRetentionPolicyPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<BucketMetadata>(
      client, EXPECT_CALL(*mock_, LockBucketRetentionPolicy),
      [](Client& client) {
        return client.LockBucketRetentionPolicy("test-bucket-name", 1U)
            .status();
      },
      "LockBucketRetentionPolicy");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
