// Copyright 2020 Google LLC
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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/setenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;

/**
 * @test Verify GrpcClient and HybridClient report failures correctly.
 */
class GrpcClientFailuresTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::string> {
 protected:
  GrpcClientFailuresTest()
      : grpc_config_("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", {}),
        rest_endpoint_("CLOUD_STORAGE_EMULATOR_ENDPOINT", {}),
        grpc_endpoint_("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                       {}),
        client_(
            storage_experimental::DefaultGrpcClient(TestOptions(GetParam()))) {}

  void SetUp() override {
    // Older versions of gRPC flake on this test, see #13114.
    std::vector<std::string> s = absl::StrSplit(grpc::Version(), '.');
    if (s.size() < 2) GTEST_SKIP();
    auto const major = std::stoi(s[0]);
    auto const minor = std::stoi(s[1]);
    if (major < 1 || (major == 1 && minor < 60)) GTEST_SKIP();
  }

  static Options TestOptions(std::string const& plugin_config) {
    using us = std::chrono::microseconds;
    return Options{}
        .set<ProjectIdOption>("project-id")
        .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(0).clone())
        .set<BackoffPolicyOption>(
            ExponentialBackoffPolicy(us(1), us(1), 2).clone())
        .set<IdempotencyPolicyOption>(AlwaysRetryIdempotencyPolicy().clone())
        .set<RestEndpointOption>("http://localhost:1")
        .set<IamEndpointOption>("http://localhost:1")
        .set<EndpointOption>("localhost:1")
        .set<Oauth2CredentialsOption>(oauth2::CreateAnonymousCredentials())
        .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
        .set<storage_experimental::GrpcPluginOption>(plugin_config);
  }

  testing_util::ScopedEnvironment grpc_config_;
  testing_util::ScopedEnvironment rest_endpoint_;
  testing_util::ScopedEnvironment grpc_endpoint_;
  Client client_;
};

TEST_P(GrpcClientFailuresTest, ListBuckets) {
  auto actual = client_.ListBuckets();
  std::vector<decltype(actual)::value_type> copy(actual.begin(), actual.end());
  EXPECT_THAT(copy, ElementsAre(StatusIs(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, CreateBucket) {
  auto actual = client_.CreateBucket("bkt", BucketMetadata());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetBucketMetadata) {
  auto actual = client_.GetBucketMetadata("bkt");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteBucket) {
  auto actual = client_.DeleteBucket("bkt");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateBucket) {
  auto actual = client_.UpdateBucket("bkt", BucketMetadata());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchBucket) {
  auto actual = client_.PatchBucket("bkt", BucketMetadataPatchBuilder());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetNativeBucketIamPolicy) {
  auto actual = client_.GetNativeBucketIamPolicy("bkt");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, SetNativeBucketIamPolicy) {
  auto actual = client_.SetNativeBucketIamPolicy(
      "bkt", NativeIamPolicy(std::vector<NativeIamBinding>{}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, TestBucketIamPermissions) {
  auto actual = client_.TestBucketIamPermissions("bkt", {});
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, LockBucketRetentionPolicy) {
  auto actual = client_.LockBucketRetentionPolicy("bkt", 0);
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaSimple) {
  auto actual =
      client_.InsertObject("bkt", "obj", "contents", DisableMD5Hash(true),
                           DisableCrc32cChecksum(true));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaMultipart) {
  auto actual = client_.InsertObject("bkt", "obj", "contents");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMedia) {
  auto actual = client_.InsertObject("bkt", "obj", "contents", Fields(""));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetObjectMetadata) {
  auto actual = client_.GetObjectMetadata("bkt", "obj");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListObjects) {
  auto actual = client_.ListObjects("bkt");
  std::vector<decltype(actual)::value_type> copy(actual.begin(), actual.end());
  EXPECT_THAT(copy, ElementsAre(StatusIs(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, DeleteObject) {
  auto actual = client_.DeleteObject("bkt", "obj");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateObject) {
  auto actual = client_.UpdateObject("bkt", "obj", ObjectMetadata());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchObject) {
  auto actual = client_.PatchObject("bkt", "obj", ObjectMetadataPatchBuilder());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ComposeObject) {
  auto actual = client_.ComposeObject("bkt", {}, "obj");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListBucketAcl) {
  auto actual = client_.ListBucketAcl("bkt");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CopyObject) {
  auto actual = client_.CopyObject("bkt", "obj1", "bkt", "obj2");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateBucketAcl) {
  auto actual = client_.CreateBucketAcl("bkt", "entity", "role");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetBucketAcl) {
  auto actual = client_.GetBucketAcl("bkt", "entity");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteBucketAcl) {
  auto actual = client_.DeleteBucketAcl("bkt", "entity");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateBucketAcl) {
  auto actual = client_.UpdateBucketAcl("bkt", BucketAccessControl());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchBucketAcl) {
  auto actual = client_.PatchBucketAcl("bkt", "entity",
                                       BucketAccessControlPatchBuilder());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListObjectAcl) {
  auto actual = client_.ListObjectAcl("bkt", "obj");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateObjectAcl) {
  auto actual = client_.CreateObjectAcl("bkt", "obj", "entity", "role");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteObjectAcl) {
  auto actual = client_.DeleteObjectAcl("bkt", "obj", "entity");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetObjectAcl) {
  auto actual = client_.GetObjectAcl("bkt", "obj", "entity");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateObjectAcl) {
  auto actual = client_.UpdateObjectAcl("bkt", "obj", ObjectAccessControl());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchObjectAcl) {
  auto actual = client_.PatchObjectAcl(
      "bkt", "obj", "entity", ObjectAccessControl(), ObjectAccessControl());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, RewriteObject) {
  auto actual = client_.RewriteObject("bkt", "obj", "bkt2", "obj2");
  EXPECT_THAT(actual.Iterate(), StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateResumableUpload) {
  auto actual = client_.WriteObject("test-bucket", "test-object");
  EXPECT_TRUE(actual.bad());
  EXPECT_THAT(actual.last_status(), StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteResumableUpload) {
  auto actual = client_.DeleteResumableUpload("test-upload-id");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListDefaultObjectAcl) {
  auto actual = client_.ListDefaultObjectAcl("bkt");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateDefaultObjectAcl) {
  auto actual = client_.CreateDefaultObjectAcl("bkt", "entity", "role");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteDefaultObjectAcl) {
  auto actual = client_.DeleteDefaultObjectAcl("bkt", "entity");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetDefaultObjectAcl) {
  auto actual = client_.GetDefaultObjectAcl("bkt", "entity");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateDefaultObjectAcl) {
  auto actual = client_.UpdateDefaultObjectAcl("bkt", ObjectAccessControl());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchDefaultObjectAcl) {
  auto actual = client_.PatchDefaultObjectAcl(
      "bkt", "entity", ObjectAccessControlPatchBuilder());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetServiceAccount) {
  auto actual = client_.GetServiceAccount();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListHmacKeys) {
  auto actual = client_.ListHmacKeys();
  std::vector<decltype(actual)::value_type> copy(actual.begin(), actual.end());
  EXPECT_THAT(copy, ElementsAre(StatusIs(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, CreateHmacKeyRequest) {
  auto actual = client_.CreateHmacKey("service-account");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, SignBlob) {
  auto actual = client_.CreateV4SignedUrl("GET", "bkt", "obj",
                                          SigningAccount("test-only@invalid"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListNotifications) {
  auto actual = client_.ListNotifications("bkt");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateNotification) {
  auto actual =
      client_.CreateNotification("bkt", "topic", NotificationMetadata());
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetNotification) {
  auto actual = client_.GetNotification("bkt", "notification_id");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteNotification) {
  auto actual = client_.DeleteNotification("bkt", "notification_id");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

INSTANTIATE_TEST_SUITE_P(HybridClientFailures, GrpcClientFailuresTest,
                         ::testing::Values("media"));

INSTANTIATE_TEST_SUITE_P(GrpcClientFailures, GrpcClientFailuresTest,
                         ::testing::Values("metadata"));

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
