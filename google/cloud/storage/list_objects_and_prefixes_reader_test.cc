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

#include "google/cloud/storage/list_objects_and_prefixes_reader.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::internal::ListObjectsRequest;
using ::google::cloud::storage::internal::ListObjectsResponse;
using ::google::cloud::storage::internal::SortObjectsAndPrefixes;
using ::google::cloud::storage::testing::MockClient;
using ::testing::_;
using ::testing::ContainerEq;

ObjectMetadata CreateElement(int index) {
  std::string id = "object-" + std::to_string(index);
  std::string name = id;
  std::string link =
      "https://storage.googleapis.com/storage/v1/b/test-bucket/" + id + "/1";
  nlohmann::json metadata{
      {"bucket", "test-bucket"},
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"kind", "storage#object"},
  };
  return internal::ObjectMetadataParser::FromJson(metadata).value();
};

TEST(ListObjectsAndPrefixesReaderTest, Basic) {
  std::vector<ObjectOrPrefix> expected;

  int page_count = 3;
  for (int i = 0; i < page_count; ++i) {
    // The returned results are sorted by page. So `expected` should be sorted
    // by page rather than the whole vector.
    expected.emplace_back(std::to_string(2 * i));
    expected.emplace_back(std::to_string(2 * i + 1));
    expected.emplace_back(CreateElement(2 * i));
    expected.emplace_back(CreateElement(2 * i + 1));
  }

  auto create_mock = [page_count](int i) {
    ListObjectsResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.emplace_back(CreateElement(2 * i));
      response.items.emplace_back(CreateElement(2 * i + 1));
      response.prefixes.emplace_back(std::to_string(2 * i));
      response.prefixes.emplace_back(std::to_string(2 * i + 1));
    }
    return [response](ListObjectsRequest const&) {
      return StatusOr<ListObjectsResponse>(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects(_))
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce(create_mock(2));

  auto reader = google::cloud::internal::MakePaginationRange<
      ListObjectsAndPrefixesReader>(
      ListObjectsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock](ListObjectsRequest const& r) { return mock->ListObjects(r); },
      [](internal::ListObjectsResponse r) {
        std::vector<ObjectOrPrefix> result;
        for (auto& item : r.items) {
          result.emplace_back(std::move(item));
        }
        for (auto& prefix : r.prefixes) {
          result.emplace_back(std::move(prefix));
        }
        SortObjectsAndPrefixes(result);
        return result;
      });
  std::vector<ObjectOrPrefix> actual;
  for (auto&& object : reader) {
    ASSERT_STATUS_OK(object);
    actual.emplace_back(std::move(object).value());
  }
  EXPECT_THAT(actual, ContainerEq(expected));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
