// Copyright 2022 Google LLC
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

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

#include "google/cloud/storage/async_client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <numeric>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace gcs = ::google::cloud::storage;
using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;
using ::testing::Not;

class AsyncClientIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
    ASSERT_THAT(bucket_name_, Not(IsEmpty()))
        << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(AsyncClientIntegrationTest, ObjectCRUD) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  auto insert = client->InsertObject(bucket_name(), object_name, LoremIpsum(),
                                     gcs::IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto async = MakeAsyncClient();
  auto pending0 =
      async.ReadObject(bucket_name(), object_name, 0, LoremIpsum().size());
  auto pending1 =
      async.ReadObject(bucket_name(), object_name, 0, LoremIpsum().size());

  for (auto* p : {&pending1, &pending0}) {
    auto response = p->get();
    EXPECT_STATUS_OK(response.status);
    auto const full = std::accumulate(response.contents.begin(),
                                      response.contents.end(), std::string{});
    EXPECT_EQ(full, LoremIpsum());
  }
  auto status = async
                    .DeleteObject(bucket_name(), object_name,
                                  gcs::Generation(insert->generation()))
                    .get();
  EXPECT_STATUS_OK(status);

  auto get = client->GetObjectMetadata(bucket_name(), object_name);
  EXPECT_THAT(get, StatusIs(StatusCode::kNotFound));
}

TEST_F(AsyncClientIntegrationTest, ComposeObject) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto o1 = MakeRandomObjectName();
  auto o2 = MakeRandomObjectName();
  auto destination = MakeRandomObjectName();

  auto insert1 = client->InsertObject(bucket_name(), o1, LoremIpsum(),
                                      gcs::IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert1);
  ScheduleForDelete(*insert1);
  auto insert2 = client->InsertObject(bucket_name(), o2, LoremIpsum(),
                                      gcs::IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert2);
  ScheduleForDelete(*insert2);

  auto async = MakeAsyncClient();
  auto pending = async.ComposeObject(
      bucket_name(),
      {storage::ComposeSourceObject{insert1->name(), insert1->generation(),
                                    absl::nullopt},
       storage::ComposeSourceObject{insert2->name(), insert2->generation(),
                                    absl::nullopt}},
      destination);
  auto const composed = pending.get();
  EXPECT_STATUS_OK(composed);
  ScheduleForDelete(*composed);

  auto read =
      async.ReadObject(bucket_name(), destination, 0, 2 * LoremIpsum().size())
          .get();
  ASSERT_STATUS_OK(read.status);
  auto const full_contents = std::accumulate(
      read.contents.begin(), read.contents.end(), std::string{});
  EXPECT_EQ(full_contents, LoremIpsum() + LoremIpsum());
  ASSERT_TRUE(read.object_metadata.has_value());
  EXPECT_EQ(*read.object_metadata, *composed);
}

TEST_F(AsyncClientIntegrationTest, WriteObject) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto o1 = MakeRandomObjectName();
  auto o2 = MakeRandomObjectName();

  auto async = MakeAsyncClient();
  auto pending0 = async.StartResumableUpload(bucket_name(), o1);
  auto pending1 = async.StartResumableUpload(bucket_name(), o2);

  for (auto* p : {&pending1, &pending0}) {
    auto response = p->get();
    EXPECT_STATUS_OK(response);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
