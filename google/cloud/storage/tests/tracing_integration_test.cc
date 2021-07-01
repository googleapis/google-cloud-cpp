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
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

class TracingIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
  }

  std::string const& bucket_name() const { return bucket_name_; }

  void UseClient(Client client, std::string const& bucket_name,
                 std::string const& object_name, std::string const& payload) {
    StatusOr<ObjectMetadata> meta = client.InsertObject(
        bucket_name, object_name, payload, IfGenerationMatch(0));
    ASSERT_THAT(meta, IsOk());
    ScheduleForDelete(*meta);

    auto stream = client.ReadObject(bucket_name, object_name);
    std::string actual(std::istreambuf_iterator<char>{stream}, {});
    EXPECT_EQ(payload, actual);
  }

 private:
  std::string bucket_name_;
};

TEST_F(TracingIntegrationTest, RawClient) {
  auto client =
      Client(Options{}.set<TracingComponentsOption>({"raw-client", "http"}));

  ScopedLog log;
  auto const object_name = MakeRandomObjectName();
  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), object_name, LoremIpsum()));
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines, Contains(HasSubstr("object_name=" + object_name)));
  EXPECT_THAT(lines, Contains(HasSubstr("/o/" + object_name)));
  EXPECT_THAT(lines, Contains(HasSubstr("curl(Info)")));
  EXPECT_THAT(lines, Contains(HasSubstr("curl(Send Header)")));
  EXPECT_THAT(lines, Contains(HasSubstr("curl(Recv Header)")));
  EXPECT_THAT(lines, Contains(HasSubstr("curl(Send Data)")));
  EXPECT_THAT(lines, Contains(HasSubstr("curl(Recv Data)")));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
