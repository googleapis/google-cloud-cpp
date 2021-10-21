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
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::internal::ListObjectsRequest;
using ::google::cloud::storage::internal::ListObjectsResponse;
using ::google::cloud::storage::testing::MockClient;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::testing::ContainerEq;
using ::testing::Return;

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

TEST(ListObjectsReaderTest, Basic) {
  // Create a synthetic list of ObjectMetadata elements, each request will
  // return 2 of them.
  std::vector<ObjectMetadata> expected;

  int page_count = 3;
  for (int i = 0; i != 2 * page_count; ++i) {
    expected.emplace_back(CreateElement(i));
  }

  auto create_mock = [page_count](int i) {
    ListObjectsResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.emplace_back(CreateElement(2 * i));
      response.items.emplace_back(CreateElement(2 * i + 1));
    }
    return [response](ListObjectsRequest const&) {
      return StatusOr<ListObjectsResponse>(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce(create_mock(2));

  auto reader = google::cloud::internal::MakePaginationRange<ListObjectsReader>(
      ListObjectsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock](ListObjectsRequest const& r) { return mock->ListObjects(r); },
      [](ListObjectsResponse r) { return std::move(r.items); });
  std::vector<ObjectMetadata> actual;
  for (auto&& object : reader) {
    ASSERT_STATUS_OK(object);
    actual.emplace_back(std::move(object).value());
  }
  EXPECT_THAT(actual, ContainerEq(expected));
}

TEST(ListObjectsReaderTest, Empty) {
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(Return(make_status_or(ListObjectsResponse{})));

  auto reader = google::cloud::internal::MakePaginationRange<ListObjectsReader>(
      ListObjectsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock](ListObjectsRequest const& r) { return mock->ListObjects(r); },
      [](ListObjectsResponse r) { return std::move(r.items); });
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(0, count);
}

TEST(ListObjectsReaderTest, PermanentFailure) {
  // Create a synthetic list of ObjectMetadata elements, each request will
  // return 2 of them.
  std::vector<ObjectMetadata> expected;

  int const page_count = 2;
  for (int i = 0; i != 2 * page_count; ++i) {
    expected.emplace_back(CreateElement(i));
  }

  auto create_mock = [&](int i) {
    ListObjectsResponse response;
    response.next_page_token = "page-" + std::to_string(i);
    response.items.emplace_back(CreateElement(2 * i));
    response.items.emplace_back(CreateElement(2 * i + 1));
    return [response](ListObjectsRequest const&) {
      return StatusOr<ListObjectsResponse>(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce([](ListObjectsRequest const&) {
        return StatusOr<ListObjectsResponse>(PermanentError());
      });

  auto reader = google::cloud::internal::MakePaginationRange<ListObjectsReader>(
      ListObjectsRequest("test-bucket"),
      [mock](ListObjectsRequest const& r) { return mock->ListObjects(r); },
      [](ListObjectsResponse r) { return std::move(r.items); });
  std::vector<ObjectMetadata> actual;
  bool has_status_or_error = false;
  for (auto&& object : reader) {
    if (object.ok()) {
      actual.emplace_back(*std::move(object));
      continue;
    }
    // The iteration should fail only once, an error should reset the iterator
    // to `end()`.
    EXPECT_FALSE(has_status_or_error);
    has_status_or_error = true;
    // Verify the error is what we expect.
    Status status = std::move(object).status();
    EXPECT_EQ(PermanentError().code(), status.code());
    EXPECT_EQ(PermanentError().message(), status.message());
  }
  // The iteration should have returned an error at least once.
  EXPECT_TRUE(has_status_or_error);

  // The iteration should have returned all the elements prior to the error.
  EXPECT_THAT(actual, ContainerEq(expected));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
