// Copyright 2018 Google LLC
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
#include "google/cloud/storage/internal/object_write_streambuf.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

class ObjectWriteStreambufIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  void CheckUpload(int line_count, int line_size) {
    auto client = MakeIntegrationTestClient();
    auto object_name = MakeRandomObjectName();

    ResumableUploadRequest request(bucket_name_, object_name);
    request.set_multiple_options(IfGenerationMatch(0));

    auto connection = internal::ClientImplDetails::GetConnection(client);
    // Normally this is done by `storage::Client`, but here we are intentionally
    // bypassing it.
    google::cloud::internal::OptionsSpan const span(connection->options());
    auto create = connection->CreateResumableUpload(request);
    ASSERT_STATUS_OK(create);

    auto constexpr kTestUploadBufferSize = 16 * 1024 * 1024L;
    ObjectWriteStream writer(std::make_unique<ObjectWriteStreambuf>(
        connection, request, std::move(create->upload_id), /*committed_size=*/0,
        /*metadata=*/absl::nullopt, kTestUploadBufferSize,
        CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
        AutoFinalizeConfig::kEnabled));

    std::ostringstream expected_stream;
    WriteRandomLines(writer, expected_stream, line_count, line_size);
    writer.Close();
    ASSERT_STATUS_OK(writer.metadata());
    ScheduleForDelete(*writer.metadata());

    ObjectReadStream reader = client.ReadObject(bucket_name_, object_name);

    std::string actual(std::istreambuf_iterator<char>{reader}, {});

    std::string expected = std::move(expected_stream).str();
    ASSERT_EQ(expected.size(), actual.size());
    EXPECT_EQ(expected, actual);
  }

  std::string bucket_name_;
};

TEST_F(ObjectWriteStreambufIntegrationTest, Simple) { CheckUpload(20, 128); }

TEST_F(ObjectWriteStreambufIntegrationTest, MultipleOfUploadQuantum) {
  CheckUpload(3 * 2 * 1024, 128);
}

TEST_F(ObjectWriteStreambufIntegrationTest, QuantumAndNonQuantum) {
  CheckUpload(3 * 1024, 128);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
