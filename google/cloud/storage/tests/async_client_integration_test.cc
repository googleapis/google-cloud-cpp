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
    if (!UsingEmulator()) GTEST_SKIP();
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

TEST_F(AsyncClientIntegrationTest, WriteObject) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto o1 = MakeRandomObjectName();
  auto o2 = MakeRandomObjectName();

  auto async = MakeAsyncClient();
  auto pending0 = async.StartResumableUpload(bucket_name(), o1);
  auto pending1 = async.StartResumableUpload(bucket_name(), o2);

  std::vector<std::string> ids;
  for (auto* p : {&pending1, &pending0}) {
    auto response = p->get();
    EXPECT_STATUS_OK(response);
    ids.push_back(*std::move(response));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
