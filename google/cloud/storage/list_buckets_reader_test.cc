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

#include "google/cloud/storage/list_buckets_reader.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
using internal::ListBucketsRequest;
using internal::ListBucketsResponse;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Invoke;
using testing::MockClient;
using ::testing::Return;
using testing::canonical_errors::PermanentError;
using testing::canonical_errors::TransientError;
namespace {
TEST(ListBucketsReaderTest, Basic) {
  // Create a synthetic list of BucketMetadata elements, each request will
  // return 2 of them.
  std::vector<BucketMetadata> expected;

  int page_count = 3;
  for (int i = 0; i != 2 * page_count; ++i) {
    std::string id = "bucket-" + std::to_string(i);
    std::string name = id;
    std::string link =
        "https://www.googleapis.com/storage/v1/b/foo-bar/" + id + "/1";
    internal::nl::json metadata{
        {"bucket", "foo-bar"},
        {"id", id},
        {"name", name},
        {"selfLink", link},
        {"kind", "storage#bucket"},
    };
    expected.emplace_back(BucketMetadata::ParseFromJson(metadata).value());
  }

  auto create_mock = [&expected, page_count](int i) {
    ListBucketsResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.push_back(expected[2 * i]);
      response.items.push_back(expected[2 * i + 1]);
    }
    return [response](ListBucketsRequest const&) {
      return make_status_or(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets(_))
      .WillOnce(Invoke(create_mock(0)))
      .WillOnce(Invoke(create_mock(1)))
      .WillOnce(Invoke(create_mock(2)));

  ListBucketsReader reader(mock, "foo-bar-baz", Prefix("dir/"));
  std::vector<BucketMetadata> actual;
  for (auto&& bucket : reader) {
    actual.push_back(bucket);
  }
  EXPECT_THAT(actual, ContainerEq(expected));
}

TEST(ListBucketsReaderTest, Empty) {
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets(_))
      .WillOnce(Return(make_status_or(ListBucketsResponse())));

  ListBucketsReader reader(mock, "foo-bar-baz", Prefix("dir/"));
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(0U, count);
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
