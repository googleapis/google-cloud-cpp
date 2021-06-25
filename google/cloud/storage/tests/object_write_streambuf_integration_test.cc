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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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
    StatusOr<Client> client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(client);
    auto object_name = MakeRandomObjectName();

    ResumableUploadRequest request(bucket_name_, object_name);
    request.set_multiple_options(IfGenerationMatch(0));

    StatusOr<std::unique_ptr<ResumableUploadSession>> session =
        internal::ClientImplDetails::GetRawClient(*client)
            ->CreateResumableSession(request);
    ASSERT_STATUS_OK(session);

    ObjectWriteStream writer(absl::make_unique<ObjectWriteStreambuf>(
        std::move(session).value(),
        internal::ClientImplDetails::GetRawClient(*client)
            ->client_options()
            .upload_buffer_size(),
        absl::make_unique<NullHashValidator>(), AutoFinalizeConfig::kEnabled));

    std::ostringstream expected_stream;
    WriteRandomLines(writer, expected_stream, line_count, line_size);
    writer.Close();
    ObjectMetadata metadata = writer.metadata().value();
    EXPECT_EQ(object_name, metadata.name());
    EXPECT_EQ(bucket_name_, metadata.bucket());

    ObjectReadStream reader = client->ReadObject(bucket_name_, object_name);

    std::string actual(std::istreambuf_iterator<char>{reader}, {});

    std::string expected = std::move(expected_stream).str();
    ASSERT_EQ(expected.size(), actual.size());
    EXPECT_EQ(expected, actual);

    auto status = client->DeleteObject(bucket_name_, object_name,
                                       Generation(metadata.generation()));
    ASSERT_STATUS_OK(status);
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
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
