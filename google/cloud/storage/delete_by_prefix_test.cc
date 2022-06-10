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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Return;

ObjectMetadata CreateObject(int index) {
  std::string id = "object-" + std::to_string(index);
  std::string name = id;
  std::string link =
      "https://storage.googleapis.com/storage/v1/b/test-bucket/" + id + "#1";
  nlohmann::json metadata{
      {"bucket", "test-bucket"},
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"generation", "1"},
      {"kind", "storage#object"},
  };
  return internal::ObjectMetadataParser::FromJson(metadata).value();
};

TEST(DeleteByPrefix, Basic) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& req)
                    -> StatusOr<internal::ListObjectsResponse> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        std::ostringstream os;
        os << req;
        EXPECT_THAT(os.str(), HasSubstr("userProject=project-to-bill"));
        EXPECT_THAT(os.str(), HasSubstr("prefix=object-"));

        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-2", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-3", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-", Versions(),
                               UserProject("project-to-bill"));
  EXPECT_STATUS_OK(status);
}

TEST(DeleteByPrefix, DeleteByPrefixNoOptions) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& req)
                    -> StatusOr<internal::ListObjectsResponse> {
        EXPECT_EQ("test-bucket", req.bucket_name());

        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-2", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-3", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-");

  EXPECT_STATUS_OK(status);
}

TEST(DeleteByPrefix, DeleteByPrefixListFailure) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(Return(StatusOr<internal::ListObjectsResponse>(
          Status(StatusCode::kPermissionDenied, ""))));
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-", Versions(),
                               UserProject("project-to-bill"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DeleteByPrefix, DeleteByPrefixDeleteFailure) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& req)
                    -> StatusOr<internal::ListObjectsResponse> {
        EXPECT_EQ("test-bucket", req.bucket_name());

        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(
          Status(StatusCode::kPermissionDenied, ""))))
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-3", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-", Versions(),
                               UserProject("project-to-bill"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DeleteByPrefix, ComposeManyNone) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = testing::ClientFromMock(mock);
  auto res =
      ComposeMany(client, "test-bucket", std::vector<ComposeSourceObject>{},
                  "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
