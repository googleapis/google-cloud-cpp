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

class ObjectWriteStreamIntegrationTest
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

TEST_F(ObjectWriteStreamIntegrationTest, MoveWorkingStream) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto constexpr kBlockSize = 256 * 1024;
  auto const block = MakeRandomData(kBlockSize);
  auto w1 =
      client->WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  ASSERT_TRUE(w1.good());

  EXPECT_TRUE(w1.write(block.data(), kBlockSize));
  EXPECT_TRUE(w1.flush());
  ASSERT_STATUS_OK(w1.last_status());

  auto w2 = std::move(w1);
  EXPECT_TRUE(w2);
  EXPECT_TRUE(w2.write(block.data(), kBlockSize));
  EXPECT_TRUE(w2.flush());
  ASSERT_STATUS_OK(w2.last_status());

  ObjectWriteStream w3;

  w3 = std::move(w2);
  EXPECT_TRUE(w3);
  EXPECT_TRUE(w3.write(block.data(), kBlockSize));
  EXPECT_TRUE(w3.flush());
  ASSERT_STATUS_OK(w3.last_status());

  w3.Close();
  ASSERT_STATUS_OK(w3.metadata());
  ScheduleForDelete(*w3.metadata());
  EXPECT_EQ(w3.metadata()->size(), 3 * kBlockSize);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
