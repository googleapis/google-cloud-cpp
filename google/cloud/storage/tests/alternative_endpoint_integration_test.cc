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
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

class AlternativeEndpointIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

struct Test {
  std::string endpoint_host;
  std::function<void(std::vector<std::string> const&)> validate;
};

std::vector<Test> CreateTests() {
  std::vector<Test> cases;
  cases.push_back(Test{
      "storage.googleapis.com", [](std::vector<std::string> const& lines) {
        EXPECT_THAT(lines, Contains(HasSubstr("Host: storage.googleapis.com")));
      }});
  auto alternatives = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_ALTERNATIVE_HOSTS");
  if (!alternatives) return cases;
  for (auto hostname : absl::StrSplit(*alternatives, ',')) {
    auto const h = std::string{hostname.data(), hostname.size()};
    cases.push_back(
        Test{h, [h](std::vector<std::string> const& lines) {
               EXPECT_THAT(lines, Not(Contains(HasSubstr("Host: " + h))));
               EXPECT_THAT(lines,
                           Contains(HasSubstr("Host: storage.googleapis.com")));
             }});
  }
  return cases;
}

void UseClientReadAndDelete(Client client, std::string const& bucket_name,
                            std::string const& object_name,
                            std::string const& payload) {
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(payload, actual);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_THAT(status, IsOk());
}

TEST_F(AlternativeEndpointIntegrationTest, Insert) {
  if (UsingEmulator()) GTEST_SKIP();

  for (auto const& test : CreateTests()) {
    SCOPED_TRACE("Testing with " + test.endpoint_host);
    testing_util::ScopedLog log;
    auto client =
        Client(Options{}
                   .set<RestEndpointOption>("https://" + test.endpoint_host)
                   .set<TracingComponentsOption>({"raw-client", "http"}));
    auto const object_name = MakeRandomObjectName();
    auto const payload = LoremIpsum();
    StatusOr<ObjectMetadata> meta = client.InsertObject(
        bucket_name(), object_name, payload, IfGenerationMatch(0));
    ASSERT_THAT(meta, IsOk());
    EXPECT_EQ(object_name, meta->name());

    UseClientReadAndDelete(client, bucket_name(), object_name, payload);
    auto lines = log.ExtractLines();
    test.validate(lines);
  }
}

TEST_F(AlternativeEndpointIntegrationTest, Write) {
  if (UsingEmulator()) GTEST_SKIP();

  for (auto const& test : CreateTests()) {
    SCOPED_TRACE("Testing with " + test.endpoint_host);
    testing_util::ScopedLog log;
    auto client =
        Client(Options{}
                   .set<RestEndpointOption>("https://" + test.endpoint_host)
                   .set<TracingComponentsOption>({"raw-client", "http"}));

    auto const object_name = MakeRandomObjectName();
    auto const payload = LoremIpsum();
    auto os =
        client.WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
    os.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    os.Close();
    auto meta = os.metadata();
    ASSERT_THAT(meta, IsOk());
    EXPECT_EQ(object_name, meta->name());

    UseClientReadAndDelete(client, bucket_name(), object_name, payload);
    auto lines = log.ExtractLines();
    test.validate(lines);
  }
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
