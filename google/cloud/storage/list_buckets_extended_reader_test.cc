// Copyright 2025 Google LLC
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

#include "google/cloud/storage/list_buckets_extended_reader.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ListBucketsRequest;
using ::google::cloud::storage::internal::ListBucketsResponse;
using ::google::cloud::storage::testing::MockClient;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
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

TEST(ListBucketsExtendedReaderTest, Basic) {
  // We will have 3 pages.
  // Page 0: 2 buckets, 1 unreachable
  // Page 1: 1 bucket, 0 unreachable
  // Page 2: 0 buckets, 1 unreachable

  std::vector<BucketsExtended> expected;

  // Page 0
  expected.push_back({{CreateElement(0), CreateElement(1)}, {"region-a"}});

  // Page 1
  expected.push_back({{CreateElement(2)}, {}});

  // Page 2
  expected.push_back({{}, {"region-b"}});

  auto create_mock = [&](int i) {
    ListBucketsResponse response;
    if (i < 2) {
      response.next_page_token = "page-" + std::to_string(i + 1);
    }
    response.items = expected[i].buckets;
    response.unreachable = expected[i].unreachable;

    return [response](ListBucketsRequest const&) {
      return make_status_or(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce(create_mock(2));

  auto reader =
      google::cloud::internal::MakePaginationRange<ListBucketsExtendedReader>(
          ListBucketsRequest("test-project"),
          [mock](ListBucketsRequest const& r) { return mock->ListBuckets(r); },
          [](internal::ListBucketsResponse r) {
            std::vector<BucketsExtended> result;
            if (r.items.empty() && r.unreachable.empty()) return result;
            result.push_back(
                BucketsExtended{std::move(r.items), std::move(r.unreachable)});
            return result;
          });

  std::vector<BucketsExtended> actual;
  for (auto&& page : reader) {
    ASSERT_STATUS_OK(page);
    actual.push_back(*std::move(page));
  }

  ASSERT_EQ(actual.size(), expected.size());
  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_THAT(actual[i].buckets, ContainerEq(expected[i].buckets));
    EXPECT_THAT(actual[i].unreachable, ContainerEq(expected[i].unreachable));
  }
}

TEST(ListBucketsExtendedReaderTest, Empty) {
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce(Return(make_status_or(ListBucketsResponse())));

  auto reader =
      google::cloud::internal::MakePaginationRange<ListBucketsExtendedReader>(
          ListBucketsRequest("test-project"),
          [mock](ListBucketsRequest const& r) { return mock->ListBuckets(r); },
          [](internal::ListBucketsResponse r) {
            std::vector<BucketsExtended> result;
            if (r.items.empty() && r.unreachable.empty()) return result;
            result.push_back(
                BucketsExtended{std::move(r.items), std::move(r.unreachable)});
            return result;
          });

  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(0, count);
}

TEST(ListBucketsExtendedReaderTest, PermanentFailure) {
  // Create a synthetic list of BucketsExtended elements.
  std::vector<BucketsExtended> expected;

  // Page 0
  expected.push_back({{CreateElement(0), CreateElement(1)}, {"region-a"}});
  // Page 1
  expected.push_back({{CreateElement(2)}, {}});

  auto create_mock = [&](int i) {
    ListBucketsResponse response;
    response.next_page_token = "page-" + std::to_string(i + 1);
    response.items = expected[i].buckets;
    response.unreachable = expected[i].unreachable;
    return [response](ListBucketsRequest const&) {
      return StatusOr<ListBucketsResponse>(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce(create_mock(0))
      .WillOnce(create_mock(1))
      .WillOnce([](ListBucketsRequest const&) {
        return StatusOr<ListBucketsResponse>(PermanentError());
      });

  auto reader =
      google::cloud::internal::MakePaginationRange<ListBucketsExtendedReader>(
          ListBucketsRequest("test-project"),
          [mock](ListBucketsRequest const& r) { return mock->ListBuckets(r); },
          [](internal::ListBucketsResponse r) {
            std::vector<BucketsExtended> result;
            if (r.items.empty() && r.unreachable.empty()) return result;
            result.push_back(
                BucketsExtended{std::move(r.items), std::move(r.unreachable)});
            return result;
          });
  std::vector<BucketsExtended> actual;
  bool has_status_or_error = false;
  for (auto&& page : reader) {
    if (page.ok()) {
      actual.emplace_back(*std::move(page));
      continue;
    }
    // The iteration should fail only once, an error should reset the iterator
    // to `end()`.
    EXPECT_FALSE(has_status_or_error);
    has_status_or_error = true;
    // Verify the error is what we expect.
    Status status = std::move(page).status();
    EXPECT_EQ(PermanentError().code(), status.code());
    EXPECT_EQ(PermanentError().message(), status.message());
  }
  // The iteration should have returned an error at least once.
  EXPECT_TRUE(has_status_or_error);

  // The iteration should have returned all the elements prior to the error.
  ASSERT_EQ(actual.size(), expected.size());
  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_THAT(actual[i].buckets, ContainerEq(expected[i].buckets));
    EXPECT_THAT(actual[i].unreachable, ContainerEq(expected[i].unreachable));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
