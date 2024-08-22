// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Return;

auto match_binding = [](NativeIamBinding const& b) {
  return AllOf(
      Property(&NativeIamBinding::role, b.role()),
      Property(&NativeIamBinding::members, ElementsAreArray(b.members())),
      Property(&NativeIamBinding::has_condition, b.has_condition()));
};

Status PermanentError() {
  // We need an error code different from `kInvalidArgument` as this is used
  // by `storage_internal::RequestProjectId()`. Some of the tests could be
  // testing the wrong thing if we used the same value.
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

/**
 * Test the functions in Storage::Client related to 'Buckets: *'.
 *
 * In general, this file should include for the APIs listed in:
 *
 * https://cloud.google.com/storage/docs/json_api/v1/buckets
 */
class BucketTest : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(BucketTest, ListBucketsNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, ListBuckets).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListBuckets(Options{});
  std::vector<StatusOr<BucketMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(StatusIs(StatusCode::kInvalidArgument)));
}

TEST_F(BucketTest, ListBucketsProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListBucketsRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListBuckets(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListBuckets(Options{});
  std::vector<StatusOr<BucketMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(BucketTest, ListBucketsProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListBucketsRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListBuckets(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListBuckets(Options{});
  std::vector<StatusOr<BucketMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(BucketTest, ListBucketsProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListBucketsRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListBuckets(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.ListBuckets(Options{}.set<ProjectIdOption>("call-project-id"));
  std::vector<StatusOr<BucketMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(BucketTest, ListBucketsProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListBucketsRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListBuckets(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.ListBuckets(OverrideDefaultProject("override-project-id"),
                         Options{}.set<ProjectIdOption>("call-project-id"));
  std::vector<StatusOr<BucketMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(BucketTest, ListBucketsForProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListBucketsRequest::project_id,
                    "explicit-argument-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListBuckets(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListBucketsForProject(
      "explicit-argument-project-id",
      OverrideDefaultProject("override-project-id"),
      Options{}.set<ProjectIdOption>("call-project-id"));
  std::vector<StatusOr<BucketMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

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

  EXPECT_CALL(*mock_, client_options()).Times(0);
  EXPECT_CALL(*mock_, CreateBucket)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce([&expected](internal::CreateBucketRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.metadata().name());
        EXPECT_EQ("US", r.metadata().location());
        EXPECT_EQ("STANDARD", r.metadata().storage_class());
        EXPECT_EQ("test-project-name", r.project_id());
        return make_status_or(expected);
      });
  auto client = testing::ClientFromMock(mock_);
  auto actual = client.CreateBucket(
      "test-bucket-name",
      BucketMetadata().set_location("US").set_storage_class("STANDARD"),
      Options{}
          .set<UserProjectOption>("u-p-test")
          .set<ProjectIdOption>("test-project-name"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, CreateBucketNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, CreateBucket).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.CreateBucket("test-bucket-name", BucketMetadata(), Options{});
  ASSERT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(BucketTest, CreateBucketProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateBucketRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateBucket(expected_request()))
      .WillOnce(Return(BucketMetadata()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.CreateBucket("test-bucket-name", BucketMetadata(), Options{});
  ASSERT_STATUS_OK(actual);
}

TEST_F(BucketTest, CreateBucketProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateBucketRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateBucket(expected_request()))
      .WillOnce(Return(BucketMetadata()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.CreateBucket("test-bucket-name", BucketMetadata(), Options{});
  ASSERT_STATUS_OK(actual);
}

TEST_F(BucketTest, CreateBucketProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateBucketRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateBucket(expected_request()))
      .WillOnce(Return(BucketMetadata()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.CreateBucket("test-bucket-name", BucketMetadata(),
                          Options{}.set<ProjectIdOption>("call-project-id"));
  ASSERT_STATUS_OK(actual);
}

TEST_F(BucketTest, CreateBucketProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateBucketRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateBucket(expected_request()))
      .WillOnce(Return(BucketMetadata()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.CreateBucket("test-bucket-name", BucketMetadata(),
                          OverrideDefaultProject("override-project-id"),
                          Options{}.set<ProjectIdOption>("call-project-id"));
  ASSERT_STATUS_OK(actual);
}

TEST_F(BucketTest, CreateBucketForProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateBucketRequest::project_id,
                    "explicit-argument-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateBucket(expected_request()))
      .WillOnce(Return(BucketMetadata()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.CreateBucketForProject(
      "test-bucket-name", "explicit-argument-project-id", BucketMetadata(),
      OverrideDefaultProject("override-project-id"),
      Options{}.set<ProjectIdOption>("call-project-id"));
  ASSERT_STATUS_OK(actual);
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
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("foo-bar-baz", r.bucket_name());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.GetBucketMetadata(
      "foo-bar-baz", Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, DeleteBucket) {
  EXPECT_CALL(*mock_, DeleteBucket)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteBucketRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("foo-bar-baz", r.bucket_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteBucket(
      "foo-bar-baz", Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(status);
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
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.metadata().name());
        EXPECT_EQ("US", r.metadata().location());
        EXPECT_EQ("STANDARD", r.metadata().storage_class());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.UpdateBucket(
      "test-bucket-name",
      BucketMetadata().set_location("US").set_storage_class("STANDARD"),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
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
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket());
        EXPECT_THAT(r.payload(), HasSubstr("STANDARD"));
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.PatchBucket(
      "test-bucket-name",
      BucketMetadataPatchBuilder().SetStorageClass("STANDARD"),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketTest, GetNativeBucketIamPolicy) {
  NativeIamBinding b0{"roles/storage.admin", {"test-user"}};
  NativeIamPolicy expected{{b0}, "XYZ=", 0};

  EXPECT_CALL(*mock_, GetNativeBucketIamPolicy)
      .WillOnce(Return(StatusOr<NativeIamPolicy>(TransientError())))
      .WillOnce([&expected](internal::GetBucketIamPolicyRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.GetNativeBucketIamPolicy(
      "test-bucket-name", Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(0, actual->version());
  EXPECT_EQ("XYZ=", actual->etag());
  EXPECT_THAT(actual->bindings(), ElementsAre(match_binding(b0)));
}

TEST_F(BucketTest, SetNativeBucketIamPolicy) {
  NativeIamBinding b0{"roles/storage.admin", {"test-user"}};
  NativeIamPolicy expected{{b0}, "XYZ=", 0};

  EXPECT_CALL(*mock_, SetNativeBucketIamPolicy)
      .WillOnce(Return(StatusOr<NativeIamPolicy>(TransientError())))
      .WillOnce(
          [&expected](internal::SetNativeBucketIamPolicyRequest const& r) {
            EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
            EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_THAT(r.json_payload(), HasSubstr("test-user"));
            return make_status_or(expected);
          });
  auto client = ClientForMock();
  auto actual = client.SetNativeBucketIamPolicy(
      "test-bucket-name", expected,
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(0, actual->version());
  EXPECT_EQ("XYZ=", actual->etag());
  EXPECT_THAT(actual->bindings(), ElementsAre(match_binding(b0)));
}

TEST_F(BucketTest, TestBucketIamPermissions) {
  internal::TestBucketIamPermissionsResponse expected;
  expected.permissions.emplace_back("storage.buckets.delete");

  EXPECT_CALL(*mock_, TestBucketIamPermissions)
      .WillOnce(Return(StatusOr<internal::TestBucketIamPermissionsResponse>(
          TransientError())))
      .WillOnce(
          [&expected](internal::TestBucketIamPermissionsRequest const& r) {
            EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
            EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_THAT(r.permissions(), ElementsAre("storage.buckets.delete"));
            return make_status_or(expected);
          });
  auto client = ClientForMock();
  auto actual = client.TestBucketIamPermissions(
      "test-bucket-name", {"storage.buckets.delete"},
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, ElementsAreArray(expected.permissions));
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
            EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
            EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_EQ(42, r.metageneration());
            return make_status_or(expected);
          });
  auto client = ClientForMock();
  auto metadata = client.LockBucketRetentionPolicy(
      "test-bucket-name", 42U, Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(expected, *metadata);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
