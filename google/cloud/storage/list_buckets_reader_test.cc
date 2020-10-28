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
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::internal::ListBucketsRequest;
using ::google::cloud::storage::internal::ListBucketsResponse;
using ::google::cloud::storage::testing::MockClient;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Return;

BucketMetadata CreateElement(int index) {
  std::string id = "bucket-" + std::to_string(index);
  std::string name = id;
  std::string link = "https://storage.googleapis.com/storage/v1/b/" + id;
  nlohmann::json metadata{
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"kind", "storage#bucket"},
  };
  return internal::BucketMetadataParser::FromJson(metadata).value();
}

TEST(ListBucketsReaderTest, Basic) {
  // Create a synthetic list of BucketMetadata elements, each request will
  // return 2 of them.
  std::vector<BucketMetadata> expected;

  int page_count = 3;
  for (int i = 0; i != 2 * page_count; ++i) {
    expected.emplace_back(CreateElement(i));
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
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce(create_mock(2));

  ListBucketsReader reader(
      ListBucketsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock](ListBucketsRequest const& r) { return mock->ListBuckets(r); });
  std::vector<BucketMetadata> actual;
  for (auto&& bucket : reader) {
    ASSERT_STATUS_OK(bucket);
    actual.push_back(*bucket);
  }
  EXPECT_THAT(actual, ContainerEq(expected));
}

TEST(ListBucketsReaderTest, Empty) {
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets(_))
      .WillOnce(Return(make_status_or(ListBucketsResponse())));

  ListBucketsReader reader(
      ListBucketsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock](ListBucketsRequest const& r) { return mock->ListBuckets(r); });
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(0, count);
}

TEST(ListBucketsReaderTest, PermanentFailure) {
  // Create a synthetic list of ObjectMetadata elements, each request will
  // return 2 of them.
  std::vector<BucketMetadata> expected;

  int const page_count = 2;
  for (int i = 0; i != 2 * page_count; ++i) {
    expected.emplace_back(CreateElement(i));
  }

  auto create_mock = [&](int i) {
    ListBucketsResponse response;
    response.next_page_token = "page-" + std::to_string(i);
    response.items.emplace_back(CreateElement(2 * i));
    response.items.emplace_back(CreateElement(2 * i + 1));
    return [response](ListBucketsRequest const&) {
      return StatusOr<ListBucketsResponse>(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets(_))
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce([](ListBucketsRequest const&) {
        return StatusOr<ListBucketsResponse>(PermanentError());
      });

  ListBucketsReader reader(
      ListBucketsRequest("test-project"),
      [mock](ListBucketsRequest const& r) { return mock->ListBuckets(r); });
  std::vector<BucketMetadata> actual;
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

TEST(ListBucketsReaderTest, IteratorCompare) {
  // Create a synthetic list of BucketMetadata elements, each request will
  // return 2 of them.
  int const page_count = 1;
  auto create_mock = [](int i, int page_count) {
    ListBucketsResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.emplace_back(CreateElement(2 * i));
      response.items.emplace_back(CreateElement(2 * i + 1));
    }
    return [response](ListBucketsRequest const&) {
      return StatusOr<ListBucketsResponse>(response);
    };
  };

  auto mock1 = std::make_shared<MockClient>();
  EXPECT_CALL(*mock1, ListBuckets(_)).WillOnce(create_mock(0, page_count));

  auto mock2 = std::make_shared<MockClient>();
  EXPECT_CALL(*mock2, ListBuckets(_)).WillOnce(create_mock(0, page_count));

  ListBucketsReader reader1(
      ListBucketsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock1](ListBucketsRequest const& r) { return mock1->ListBuckets(r); });
  ListBucketsIterator a1 = reader1.begin();
  ListBucketsIterator b1 = a1;
  ListBucketsIterator e1 = reader1.end();
  EXPECT_EQ(b1, a1);
  ++b1;
  EXPECT_NE(b1, a1);
  EXPECT_NE(a1, e1);
  EXPECT_NE(b1, e1);
  ++b1;
  EXPECT_EQ(b1, e1);

  ListBucketsReader reader2(
      ListBucketsRequest("foo-bar-baz").set_multiple_options(Prefix("dir/")),
      [mock2](ListBucketsRequest const& r) { return mock2->ListBuckets(r); });
  ListBucketsIterator a2 = reader2.begin();

  // Verify that iterators from different streams, even when pointing to the
  // same elements are different.
  EXPECT_NE(a1, a2);
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
