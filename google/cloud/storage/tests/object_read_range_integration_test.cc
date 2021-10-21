// Copyright 2021 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;

class ObjectReadRangeIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 public:
  ObjectReadRangeIntegrationTest() = default;

 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(ObjectReadRangeIntegrationTest, ReadRangeSmall) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const contents = LoremIpsum();
  auto const object_name = MakeRandomObjectName();

  auto insert = client->InsertObject(bucket_name(), object_name, contents,
                                     IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  auto const size = static_cast<std::int64_t>(insert->size());

  // Read several small portions of the object, expecting specific results.
  struct Test {
    std::int64_t begin;
    std::int64_t end;
    std::string expected;
  } cases[] = {
      {0, 1, contents.substr(0, 1)},
      {4, 8, contents.substr(4, 4)},
      {0, size, contents},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [" + std::to_string(test.begin) + "," +
                 std::to_string(test.end) + ")");
    auto reader = client->ReadObject(bucket_name(), object_name,
                                     ReadRange(test.begin, test.end));
    auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_EQ(test.expected, actual);
  }
}

TEST_F(ObjectReadRangeIntegrationTest, ReadFromOffset) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const contents = LoremIpsum();
  auto const object_name = MakeRandomObjectName();

  auto insert = client->InsertObject(bucket_name(), object_name, contents,
                                     IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  auto const size = static_cast<std::int64_t>(insert->size());

  // Read several small portions of the object, expecting specific results.
  struct Test {
    std::int64_t offset;
    std::string expected;
  } cases[] = {
      {0, contents.substr(0)},
      {4, contents.substr(4)},
      {size - 1, contents.substr(contents.size() - 1)},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [" + std::to_string(test.offset) + ",end)");
    auto reader = client->ReadObject(bucket_name(), object_name,
                                     ReadFromOffset(test.offset));
    auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_EQ(test.expected, actual);
  }
}

TEST_F(ObjectReadRangeIntegrationTest, ReadLast) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const contents = LoremIpsum();
  auto const object_name = MakeRandomObjectName();

  auto insert = client->InsertObject(bucket_name(), object_name, contents,
                                     IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  auto const size = static_cast<std::int64_t>(insert->size());

  // Read several small portions of the object, expecting specific results.
  struct Test {
    std::int64_t offset;
    std::string expected;
  } cases[] = {
      {1, contents.substr(contents.size() - 1)},
      {4, contents.substr(contents.size() - 4)},
      {size, contents},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [-" + std::to_string(test.offset) + ",end)");
    auto reader =
        client->ReadObject(bucket_name(), object_name, ReadLast(test.offset));
    auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_EQ(test.expected, actual);
  }
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
