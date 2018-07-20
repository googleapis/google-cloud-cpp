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

#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
namespace nl = internal::nl;
using internal::ListObjectsRequest;
using internal::ListObjectsResponse;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Invoke;
using testing::MockClient;
using ::testing::Return;

TEST(ListObjectsReaderTest, Basic) {
  // Create a synthetic list of ObjectMetadata elements, each request will
  // return 2 of them.
  std::vector<ObjectMetadata> expected;

  int page_count = 3;
  for (int i = 0; i != 2 * page_count; ++i) {
    std::string id = "object-" + std::to_string(i);
    std::string name = id;
    std::string link =
        "https://www.googleapis.com/storage/v1/b/foo-bar/" + id + "/1";
    nl::json metadata{
        {"bucket", "foo-bar"},
        {"id", id},
        {"name", name},
        {"selfLink", link},
        {"kind", "storage#object"},
    };
    expected.emplace_back(ObjectMetadata::ParseFromJson(metadata));
  }

  auto create_mock = [&expected, page_count](int i) {
    ListObjectsResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.push_back(expected[2 * i]);
      response.items.push_back(expected[2 * i + 1]);
    }
    return [response](ListObjectsRequest const&) {
      return std::make_pair(Status(), response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects(_))
      .WillOnce(Invoke(create_mock(0)))
      .WillOnce(Invoke(create_mock(1)))
      .WillOnce(Invoke(create_mock(2)));

  ListObjectsReader reader(mock, "foo-bar-baz", Prefix("dir/"));
  std::vector<ObjectMetadata> actual;
  for (auto&& object : reader) {
    actual.push_back(object);
  }
  EXPECT_THAT(actual, ContainerEq(expected));
}

TEST(ListObjectsReaderTest, Empty) {
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects(_))
      .WillOnce(Return(std::make_pair(Status(), ListObjectsResponse())));

  ListObjectsReader reader(mock, "foo-bar-baz", Prefix("dir/"));
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(0U, count);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
