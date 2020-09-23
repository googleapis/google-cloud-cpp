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

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace storage {
namespace examples {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

ObjectMetadata CreateObject(std::string const& name, int generation) {
  nlohmann::json metadata{
      {"bucket", "fake-bucket"},
      {"name", name},
      {"generation", generation},
      {"kind", "storage#object"},
  };
  return internal::ObjectMetadataParser::FromJson(metadata).value();
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

}  // namespace
}  // namespace examples
}  // namespace storage
}  // namespace cloud
}  // namespace google
