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

#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/curl_resumable_streambuf.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

class ResumableStreambufTestEnvironment : public ::testing::Environment {
 public:
  ResumableStreambufTestEnvironment(std::string instance) {
    bucket_name_ = std::move(instance);
  }

  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string bucket_name_;
};

std::string ResumableStreambufTestEnvironment::bucket_name_;

class CurlResumableStreambufIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void CheckUpload(int line_count, int line_size) {
    Client client;
    auto bucket_name = ResumableStreambufTestEnvironment::bucket_name();
    auto object_name = MakeRandomObjectName();

    ResumableUploadRequest request(bucket_name, object_name);
    request.set_multiple_options(IfGenerationMatch(0));

    StatusOr<std::unique_ptr<ResumableUploadSession>> session =
        client.raw_client()->CreateResumableSession(request);
    ASSERT_TRUE(session.ok());

    ObjectWriteStream writer(
        google::cloud::internal::make_unique<CurlResumableStreambuf>(
            std::move(session).value(),
            client.raw_client()->client_options().upload_buffer_size(),
            google::cloud::internal::make_unique<NullHashValidator>()));

    std::ostringstream expected_stream;
    WriteRandomLines(writer, expected_stream, line_count, line_size);
    writer.Close();
    ObjectMetadata metadata = writer.metadata().value();
    EXPECT_EQ(object_name, metadata.name());
    EXPECT_EQ(bucket_name, metadata.bucket());

    ObjectReadStream reader = client.ReadObject(bucket_name, object_name);

    std::string actual(std::istreambuf_iterator<char>{reader}, {});

    std::string expected = std::move(expected_stream).str();
    ASSERT_EQ(expected.size(), actual.size());
    EXPECT_EQ(expected, actual);

    client.DeleteObject(bucket_name, object_name,
                        Generation(metadata.generation()));
  }
};


TEST_F(CurlResumableStreambufIntegrationTest, Simple) {
  CheckUpload(20, 128);
}

TEST_F(CurlResumableStreambufIntegrationTest, MultipleOfUploadQuantum) {
  CheckUpload(3 * 2 * 1024, 128);
}

TEST_F(CurlResumableStreambufIntegrationTest, QuantumAndNonQuantum) {
  CheckUpload(3 * 1024, 128);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <bucket-name>"
              << std::endl;
    return 1;
  }

  std::string const bucket_name = argv[1];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::internal::ResumableStreambufTestEnvironment(
          bucket_name));

  return RUN_ALL_TESTS();
}
