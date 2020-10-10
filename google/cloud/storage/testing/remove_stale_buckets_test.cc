// Copyright 2020 Google LLC
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

#include "google/cloud/storage/testing/remove_stale_buckets.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace storage {
namespace testing {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StartsWith;

ObjectMetadata CreateObject(std::string const& name, int generation) {
  nlohmann::json metadata{
      {"bucket", "fake-bucket"},
      {"name", name},
      {"generation", generation},
      {"kind", "storage#object"},
  };
  return internal::ObjectMetadataParser::FromJson(metadata).value();
};

BucketMetadata CreateBucket(std::string const& name,
                            std::chrono::system_clock::time_point tp) {
  nlohmann::json metadata{
      {"name", name},
      {"timeCreated", google::cloud::internal::FormatRfc3339(tp)},
      {"kind", "storage#bucket"},
  };
  return internal::BucketMetadataParser::FromJson(metadata).value();
};

TEST(CleanupStaleBucketsTest, RemoveBucketContents) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, DeleteBucket)
      .Times(1)
      .WillRepeatedly(Return(internal::EmptyResponse{}));
  EXPECT_CALL(*mock, DeleteObject)
      .Times(4)
      .WillRepeatedly(Return(internal::EmptyResponse{}));
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& r) {
        EXPECT_EQ(r.bucket_name(), "fake-bucket");
        EXPECT_TRUE(r.HasOption<Versions>());
        internal::ListObjectsResponse response;
        response.items.push_back(CreateObject("foo", 1));
        response.items.push_back(CreateObject("foo", 2));
        response.items.push_back(CreateObject("bar", 1));
        response.items.push_back(CreateObject("baz", 1));
        return response;
      });
  Client client(mock, Client::NoDecorations{});
  auto const actual = RemoveBucketAndContents(client, "fake-bucket");
  EXPECT_THAT(actual, StatusIs(StatusCode::kOk));
}

TEST(CleanupStaleBucketsTest, RemoveStaleBuckets) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, DeleteBucket)
      .Times(2)
      .WillRepeatedly(Return(internal::EmptyResponse{}));
  EXPECT_CALL(*mock, ListObjects)
      .Times(2)
      .WillRepeatedly([](internal::ListObjectsRequest const& r) {
        EXPECT_THAT(r.bucket_name(), StartsWith("matching-2020-09-21_"));
        EXPECT_TRUE(r.HasOption<Versions>());
        return internal::ListObjectsResponse{};
      });
  auto const options =
      ClientOptions{oauth2::CreateAnonymousCredentials()}.set_project_id(
          "fake-project");
  EXPECT_CALL(*mock, client_options).WillRepeatedly(ReturnRef(options));

  auto const now =
      google::cloud::internal::ParseRfc3339("2020-09-23T12:34:56Z");
  auto const create_time_limit = now - std::chrono::hours(48);
  auto const affected_tp = create_time_limit - std::chrono::hours(1);
  auto const unaffected_tp = create_time_limit + std::chrono::hours(1);

  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce([&](internal::ListBucketsRequest const& r) {
        EXPECT_EQ("fake-project", r.project_id());
        internal::ListBucketsResponse response;
        response.items.push_back(CreateBucket("not-matching", affected_tp));
        response.items.push_back(
            CreateBucket("matching-2020-09-21_0", affected_tp));
        response.items.push_back(
            CreateBucket("matching-2020-09-21_1", affected_tp));
        response.items.push_back(
            CreateBucket("matching-2020-09-21_2", unaffected_tp));
        return response;
      });

  Client client(mock, Client::NoDecorations{});
  auto const actual = RemoveStaleBuckets(client, "matching", create_time_limit);
  EXPECT_THAT(actual, StatusIs(StatusCode::kOk));
}

}  // namespace
}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
