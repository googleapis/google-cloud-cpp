// Copyright 2019 Google LLC
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

#include "google/cloud/storage/list_hmac_keys_reader.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::internal::ListHmacKeysRequest;
using ::google::cloud::storage::internal::ListHmacKeysResponse;
using ::google::cloud::storage::testing::MockClient;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Invoke;
using ::testing::Return;

HmacKeyMetadata CreateElement(int index) {
  std::string id = "bucket-" + std::to_string(index);
  std::string name = id;
  std::string link = "https://www.googleapis.com/storage/v1/b/" + id;
  internal::nl::json metadata{
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"kind", "storage#bucket"},
  };
  return internal::HmacKeyMetadataParser::FromJson(metadata).value();
}

TEST(ListHmacKeysReaderTest, Basic) {
  // Create a synthetic list of HmacKeyMetadata elements, each request will
  // return 2 of them.
  std::vector<HmacKeyMetadata> expected;

  int page_count = 3;
  for (int i = 0; i != 2 * page_count; ++i) {
    expected.emplace_back(CreateElement(i));
  }

  auto create_mock = [&expected, page_count](int i) {
    ListHmacKeysResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.push_back(expected[2 * i]);
      response.items.push_back(expected[2 * i + 1]);
    }
    return [response](ListHmacKeysRequest const&) {
      return make_status_or(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListHmacKeys(_))
      .WillOnce(Invoke(create_mock(0)))
      .WillOnce(Invoke(create_mock(1)))
      .WillOnce(Invoke(create_mock(2)));

  ListHmacKeysReader reader(
      ListHmacKeysRequest("test-project-id"),
      [mock](ListHmacKeysRequest const& r) { return mock->ListHmacKeys(r); });
  std::vector<HmacKeyMetadata> actual;
  for (auto&& bucket : reader) {
    ASSERT_STATUS_OK(bucket);
    actual.push_back(*bucket);
  }
  EXPECT_THAT(actual, ContainerEq(expected));
}

TEST(ListHmacKeysReaderTest, Empty) {
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListHmacKeys(_))
      .WillOnce(Return(make_status_or(ListHmacKeysResponse())));

  ListHmacKeysReader reader(
      ListHmacKeysRequest("test-project-id"),
      [mock](ListHmacKeysRequest const& r) { return mock->ListHmacKeys(r); });
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(0, count);
}

TEST(ListHmacKeysReaderTest, PermanentFailure) {
  // Create a synthetic list of ObjectMetadata elements, each request will
  // return 2 of them.
  std::vector<HmacKeyMetadata> expected;

  int const page_count = 2;
  for (int i = 0; i != 2 * page_count; ++i) {
    expected.emplace_back(CreateElement(i));
  }

  auto create_mock = [&](int i) {
    ListHmacKeysResponse response;
    response.next_page_token = "page-" + std::to_string(i);
    response.items.emplace_back(CreateElement(2 * i));
    response.items.emplace_back(CreateElement(2 * i + 1));
    return [response](ListHmacKeysRequest const&) {
      return StatusOr<ListHmacKeysResponse>(response);
    };
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListHmacKeys(_))
      .WillOnce(Invoke(create_mock(0)))
      .WillOnce(Invoke(create_mock(1)))
      .WillOnce(Invoke([](ListHmacKeysRequest const&) {
        return StatusOr<ListHmacKeysResponse>(PermanentError());
      }));

  ListHmacKeysReader reader(
      ListHmacKeysRequest("test-project"),
      [mock](ListHmacKeysRequest const& r) { return mock->ListHmacKeys(r); });
  std::vector<HmacKeyMetadata> actual;
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

TEST(ListHmacKeysReaderTest, IteratorCompare) {
  // Create a synthetic list of HmacKeyMetadata elements, each request will
  // return 2 of them.
  int const page_count = 1;
  auto create_mock = [](int i, int page_count) {
    ListHmacKeysResponse response;
    if (i < page_count) {
      if (i != page_count - 1) {
        response.next_page_token = "page-" + std::to_string(i);
      }
      response.items.emplace_back(CreateElement(2 * i));
      response.items.emplace_back(CreateElement(2 * i + 1));
    }
    return [response](ListHmacKeysRequest const&) {
      return StatusOr<ListHmacKeysResponse>(response);
    };
  };

  auto mock1 = std::make_shared<MockClient>();
  EXPECT_CALL(*mock1, ListHmacKeys(_))
      .WillOnce(Invoke(create_mock(0, page_count)));

  auto mock2 = std::make_shared<MockClient>();
  EXPECT_CALL(*mock2, ListHmacKeys(_))
      .WillOnce(Invoke(create_mock(0, page_count)));

  ListHmacKeysReader reader1(
      ListHmacKeysRequest("test-project-id"),
      [mock1](ListHmacKeysRequest const& r) { return mock1->ListHmacKeys(r); });
  ListHmacKeysIterator a1 = reader1.begin();
  ListHmacKeysIterator b1 = a1;
  ListHmacKeysIterator e1 = reader1.end();
  EXPECT_EQ(b1, a1);
  ++b1;
  EXPECT_NE(b1, a1);
  EXPECT_NE(a1, e1);
  EXPECT_NE(b1, e1);
  ++b1;
  EXPECT_EQ(b1, e1);

  ListHmacKeysReader reader2(
      ListHmacKeysRequest("test-project-id"),
      [mock2](ListHmacKeysRequest const& r) { return mock2->ListHmacKeys(r); });
  ListHmacKeysIterator a2 = reader2.begin();

  // Verify that iterators from different streams, even when pointing to the
  // same elements are different.
  EXPECT_NE(a1, a2);
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
