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
      .WillOnce(Return(std::make_pair(TransientError(), std::string{})))
      .WillOnce(Invoke([&expected](ReadObjectRangeRequest const& r) {
        EXPECT_EQ("foo-bar", r.bucket_name());
        EXPECT_EQ("baz.txt", r.object_name());
        return std::make_pair(storage::Status(), expected);
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
      .WillOnce(Return(std::make_pair(TransientError(), std::string{})))
      .WillOnce(Return(std::make_pair(TransientError(), std::string{})))
      .WillOnce(Return(std::make_pair(TransientError(), std::string{})));
  EXPECT_THROW(std::string(std::istreambuf_iterator<char>{actual}, {}),
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillRepeatedly(Return(std::make_pair(PermanentError(), std::string{})));
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
      .WillOnce(Return(std::make_pair(PermanentError(), std::string{})));
  EXPECT_THROW(std::string(std::istreambuf_iterator<char>{actual}, {}),
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, ReadObjectRangeMedia(_))
      .WillRepeatedly(Return(std::make_pair(PermanentError(), std::string{})));
  EXPECT_DEATH_IF_SUPPORTED(
      std::string(std::istreambuf_iterator<char>{actual}, {}),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
