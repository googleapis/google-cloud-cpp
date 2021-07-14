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
#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

class ObjectReadStreamIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(ObjectReadStreamIntegrationTest, MoveWorkingStream) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto constexpr kBlockSize = 128 * 1024;
  auto constexpr kBlockCount = 16;
  auto const block = MakeRandomData(kBlockSize);
  auto writer =
      client->WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  for (int i = 0; i != kBlockCount; ++i) {
    if (!writer.write(block.data(), kBlockSize)) break;
  }
  writer.Close();
  ASSERT_STATUS_OK(writer.metadata());
  ScheduleForDelete(*writer.metadata());

  auto r1 = client->ReadObject(bucket_name(), object_name);
  ASSERT_TRUE(r1.good());

  std::vector<char> buffer(kBlockSize);
  EXPECT_TRUE(r1.read(buffer.data(), kBlockSize));
  EXPECT_TRUE(r1.good());

  auto r2 = std::move(r1);
  EXPECT_TRUE(r2.good());
  EXPECT_TRUE(r2.read(buffer.data(), kBlockSize));
  EXPECT_TRUE(r2.good());
  EXPECT_EQ(r2.gcount(), kBlockSize);

  ObjectReadStream r3;
  r3 = std::move(r2);
  EXPECT_TRUE(r3.good());
  EXPECT_TRUE(r3.read(buffer.data(), kBlockSize));
  EXPECT_TRUE(r3.good());
  EXPECT_EQ(r3.gcount(), kBlockSize);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
