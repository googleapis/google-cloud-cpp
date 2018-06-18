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

#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

using namespace storage;
using namespace ::testing;
using storage::internal::ReadObjectRangeRequest;
using storage::internal::ReadObjectRangeResponse;
using storage::testing::MockClient;
using namespace storage::testing::canonical_errors;

TEST(ObjectStreamTest, ReadSmall) {
  std::string expected = R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
})""";

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillOnce(
          Return(std::make_pair(TransientError(), ReadObjectRangeResponse{})))
      .WillOnce(Invoke([&expected](ReadObjectRangeRequest const& r) {
        EXPECT_EQ("foo-bar", r.bucket_name());
        EXPECT_EQ("baz.txt", r.object_name());
        return std::make_pair(
            storage::Status(),
            ReadObjectRangeResponse{
                expected, 0, static_cast<std::int64_t>(expected.size()) - 1,
                static_cast<std::int64_t>(expected.size())});
      }));

  ReadObjectRangeRequest request("foo-bar", "baz.txt");
  ObjectReadStream actual(mock, std::move(request));

  std::string contents(std::istreambuf_iterator<char>{actual}, {});
  EXPECT_EQ(expected, contents);
}

TEST(ObjectStreamTest, ReadTooManyFailures) {
  auto mock = std::make_shared<MockClient>();
  ReadObjectRangeRequest request("foo-bar", "baz.txt");
  ObjectReadStream actual(mock, std::move(request));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillOnce(
          Return(std::make_pair(TransientError(), ReadObjectRangeResponse{})))
      .WillOnce(
          Return(std::make_pair(TransientError(), ReadObjectRangeResponse{})))
      .WillOnce(
          Return(std::make_pair(TransientError(), ReadObjectRangeResponse{})));
  EXPECT_THROW(std::string(std::istreambuf_iterator<char>{actual}, {}),
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), ReadObjectRangeResponse{})));
  EXPECT_DEATH_IF_SUPPORTED(
      std::string(std::istreambuf_iterator<char>{actual}, {}),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ObjectStreamTest, ReadPermanentFailure) {
  auto mock = std::make_shared<MockClient>();
  ReadObjectRangeRequest request("foo-bar", "baz.txt");
  ObjectReadStream actual(mock, std::move(request));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillOnce(
          Return(std::make_pair(PermanentError(), ReadObjectRangeResponse{})));
  EXPECT_THROW(std::string(std::istreambuf_iterator<char>{actual}, {}),
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), ReadObjectRangeResponse{})));
  EXPECT_DEATH_IF_SUPPORTED(
      std::string(std::istreambuf_iterator<char>{actual}, {}),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ObjectStreamTest, ReadLarge) {
  std::int64_t const object_size = 128 * 1024 + 12345;

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillRepeatedly(Invoke([](ReadObjectRangeRequest const& r) {
        if (r.begin() > object_size) {
          return std::make_pair(storage::Status(416, "invalid range"),
                                ReadObjectRangeResponse{});
        }
        // Return just a bunch of spaces.
        auto size = static_cast<std::size_t>(r.end() - r.begin());
        if (r.end() > object_size) {
          size = static_cast<std::size_t>(object_size - r.begin());
        }
        ReadObjectRangeResponse response{
            std::string(static_cast<std::size_t>(size), ' '), r.begin(),
            r.end() - 1, object_size};
        return std::make_pair(storage::Status(), std::move(response));
      }));

  ObjectReadStream actual(mock, ReadObjectRangeRequest("foo-bar", "baz.txt"));

  std::streamsize received_bytes = 0;
  while (not actual.eof()) {
    char buffer[4096];
    actual.read(buffer, sizeof(buffer));
    auto actual_count = actual.gcount();
    EXPECT_LE(0, actual_count) << "actual_count=" << actual.gcount();
    received_bytes += actual_count;
    SUCCEED() << "received_bytes=" << received_bytes;
  }
  EXPECT_EQ(object_size, received_bytes);
}
