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

using ::testing::Return;

ObjectMetadata MockObject(std::string const& bucket_name,
                          std::string const& object_name, int generation) {
  std::stringstream text;
  text << R"""({
        "contentDisposition": "a-disposition",
        "contentLanguage": "a-language",
        "contentType": "application/octet-stream",
        "crc32c": "d1e2f3",
        "etag": "XYZ=",
        "kind": "storage#object",
        "md5Hash": "xa1b2c3==",
        "mediaLink": "https://storage.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
        "metageneration": "4",
        "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
        "size": 1024,
        "storageClass": "STANDARD",
        "timeCreated": "2018-05-19T19:31:14Z",
        "timeDeleted": "2018-05-19T19:32:24Z",
        "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
        "updated": "2018-05-19T19:31:24Z",
)""";
  text << R"("bucket": ")" << bucket_name << "\","
       << R"("generation": ")" << generation << "\","
       << R"("id": ")" << bucket_name << "/" << object_name << "/" << generation
       << "\","
       << R"("name": ")" << object_name << "\"}";
  return internal::ObjectMetadataParser::FromString(text.str()).value();
}

TEST(ComposeMany, One) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];
        EXPECT_EQ(1, source_objects.size());
        EXPECT_EQ(42, source_objects[0]["generation"]);
        EXPECT_EQ("1", source_objects[0]["name"]);

        return MockObject("test-bucket", "test-object", 42);
      });
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = ComposeMany(
      client, "test-bucket",
      std::vector<ComposeSourceObject>{ComposeSourceObject{"1", 42, {}}},
      "prefix", "dest", false);
  EXPECT_STATUS_OK(status);
}

TEST(ComposeMany, Three) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];
        EXPECT_EQ(3, source_objects.size());
        EXPECT_EQ(42, source_objects[0]["generation"]);
        EXPECT_EQ("1", source_objects[0]["name"]);
        EXPECT_EQ(43, source_objects[1]["generation"]);
        EXPECT_EQ("2", source_objects[1]["name"]);
        EXPECT_EQ(44, source_objects[2]["generation"]);
        EXPECT_EQ("3", source_objects[2]["name"]);

        return MockObject("test-bucket", "test-object", 42);
      });
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = ComposeMany(
      client, "test-bucket",
      std::vector<ComposeSourceObject>{ComposeSourceObject{"1", 42, {}},
                                       ComposeSourceObject{"2", 43, {}},
                                       ComposeSourceObject{"3", 44, {}}},
      "prefix", "dest", false);
  EXPECT_STATUS_OK(status);
}

TEST(ComposeMany, ThreeLayers) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-0", req.object_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];

        EXPECT_EQ(32, source_objects.size());

        for (int i = 0; i != 32; ++i) {
          EXPECT_EQ(std::to_string(i), source_objects[i]["name"]);
        }

        return MockObject(req.bucket_name(), req.object_name(), 42);
      })
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-1", req.object_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];

        EXPECT_EQ(31, source_objects.size());

        for (int i = 0; i != 31; ++i) {
          EXPECT_EQ(std::to_string(i + 32), source_objects[i]["name"]);
        }

        return MockObject(req.bucket_name(), req.object_name(), 42);
      })
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        EXPECT_EQ("dest", req.object_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];

        EXPECT_EQ(2, source_objects.size());
        EXPECT_EQ("prefix.compose-tmp-0", source_objects[0]["name"]);
        EXPECT_EQ("prefix.compose-tmp-1", source_objects[1]["name"]);

        return MockObject(req.bucket_name(), req.object_name(), 42);
      });
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-0", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;

  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", false);
  EXPECT_STATUS_OK(res);
  EXPECT_EQ("dest", res->name());
}

TEST(ComposeMany, ComposeFails) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources - second composition fails.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-0", 42))))
      .WillOnce(Return(
          StatusOr<ObjectMetadata>(Status(StatusCode::kPermissionDenied, ""))));

  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });

  // Cleanup is still expected
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-0", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;
  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
}

TEST(ComposeMany, CleanupFailsLoudly) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources - second composition fails.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-0", 42))))
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-1", 42))))
      .WillOnce(Return(make_status_or(MockObject("test-bucket", "dest", 42))));

  // Cleanup is still expected
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(
          Status(StatusCode::kPermissionDenied, ""))));
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;
  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
}

TEST(ComposeMany, CleanupFailsSilently) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources - second composition fails.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-0", 42))))
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-1", 42))))
      .WillOnce(Return(make_status_or(MockObject("test-bucket", "dest", 42))));

  // Cleanup is still expected
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(
          Status(StatusCode::kPermissionDenied, ""))));

  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;
  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", true);
  EXPECT_STATUS_OK(res);
  EXPECT_EQ("dest", res->name());
}

TEST(ComposeMany, LockingPrefixFails) {
  auto mock = std::make_shared<testing::MockClient>();

  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce(Return(
          Status(StatusCode::kFailedPrecondition, "Generation mismatch")));
  auto client = testing::ClientFromMock(mock);
  auto res = ComposeMany(
      client, "test-bucket",
      std::vector<ComposeSourceObject>{ComposeSourceObject{"1", 42, {}}},
      "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kFailedPrecondition, res.status().code());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
