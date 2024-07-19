// Copyright 2021 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;

class AutoFinalizeIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(AutoFinalizeIntegrationTest, DefaultIsEnabled) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  {
    auto stream =
        client.WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
    stream << expected_text;
  }
  auto reader = client.ReadObject(bucket_name(), object_name);
  ASSERT_TRUE(reader.good());
  ASSERT_STATUS_OK(reader.status());
  auto const actual =
      std::string{std::istreambuf_iterator<char>{reader.rdbuf()}, {}};
  EXPECT_EQ(expected_text, actual);

  (void)client.DeleteObject(bucket_name(), object_name);
}

TEST_F(AutoFinalizeIntegrationTest, ExplicitlyEnabled) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  {
    auto stream =
        client.WriteObject(bucket_name(), object_name, IfGenerationMatch(0),
                           AutoFinalizeEnabled());
    stream << expected_text;
  }
  auto reader = client.ReadObject(bucket_name(), object_name);
  EXPECT_TRUE(reader.good());
  EXPECT_STATUS_OK(reader.status());
  auto const actual =
      std::string{std::istreambuf_iterator<char>{reader.rdbuf()}, {}};
  EXPECT_EQ(expected_text, actual);

  (void)client.DeleteObject(bucket_name(), object_name);
}

TEST_F(AutoFinalizeIntegrationTest, Disabled) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto constexpr kQuantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto constexpr kSize = 8 * kQuantum;
  auto const expected_text = MakeRandomData(kSize);
  auto const upload_session = [&] {
    auto os = client.WriteObject(bucket_name(), object_name,
                                 IfGenerationMatch(0), AutoFinalizeDisabled());
    auto id = os.resumable_session_id();
    os.write(expected_text.data(), kQuantum);
    os << std::flush;
    return id;
  }();

  {
    // The upload is not finalized, so the object should not exist:
    auto reader = client.ReadObject(bucket_name(), object_name);
    ASSERT_THAT(reader.status(), StatusIs(StatusCode::kNotFound));
  }

  for (std::uint64_t from = 0; kQuantum <= (kSize - from);) {
    auto os =
        client.WriteObject(bucket_name(), object_name, AutoFinalizeDisabled(),
                           UseResumableUploadSession(upload_session));
    from = os.next_expected_byte();
    if (from >= kSize) break;
    os.write(expected_text.data() + from, kQuantum);
    os << std::flush;
    EXPECT_TRUE(os.good());
    EXPECT_THAT(os.last_status(), IsOk());
  }
  auto os =
      client.WriteObject(bucket_name(), object_name, AutoFinalizeDisabled(),
                         UseResumableUploadSession(upload_session));
  os.Close();
  ASSERT_THAT(os.metadata(), IsOk());
  ScheduleForDelete(*os.metadata());
  EXPECT_EQ(os.metadata()->size(), kSize);

  auto reader = client.ReadObject(bucket_name(), object_name);
  EXPECT_TRUE(reader.good());
  EXPECT_STATUS_OK(reader.status());
  auto const actual =
      std::string{std::istreambuf_iterator<char>{reader.rdbuf()}, {}};
  EXPECT_EQ(expected_text, actual);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
