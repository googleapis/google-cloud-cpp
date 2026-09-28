// Copyright 2026 Google LLC
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
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Eq;

class ManagedWorkloadIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_THAT(bucket_name_, ::testing::Not(::testing::IsEmpty()))
        << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME environment variable "
           "must be set.";

    auto generator =
        google::cloud::internal::DefaultPRNG(std::random_device{}());
    random_suffix_ = google::cloud::internal::Sample(
        generator, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  }

  std::string bucket_name_;
  std::string random_suffix_;
};

TEST_F(ManagedWorkloadIntegrationTest, RestTransportCrud) {
  auto client = google::cloud::storage::Client();
  std::string const object_name = "mwlid-test-rest-" + random_suffix_ + ".txt";
  std::string const expected_content =
      "Hello from Cloud Run REST transport with Agent Identity!";

  std::cout << "Starting REST transport GCS CRUD test on bucket: "
            << bucket_name_ << ", object: " << object_name << std::endl;

  // 1. Write object
  auto writer = client.WriteObject(bucket_name_, object_name);
  writer << expected_content;
  writer.Close();
  ASSERT_THAT(writer.metadata(), IsOk());

  // 2. Read object
  auto reader = client.ReadObject(bucket_name_, object_name);
  ASSERT_TRUE(reader.good());
  std::string actual_content(std::istreambuf_iterator<char>(reader), {});
  ASSERT_THAT(actual_content, Eq(expected_content));

  // 3. Get metadata
  StatusOr<ObjectMetadata> metadata =
      client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_THAT(metadata, IsOk());
  EXPECT_THAT(metadata->name(), Eq(object_name));

  // 4. Delete object
  Status delete_status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_THAT(delete_status, IsOk());

  std::cout << "REST transport GCS CRUD test PASSED successfully!" << std::endl;
}

TEST_F(ManagedWorkloadIntegrationTest, GrpcTransportCrud) {
  auto client = google::cloud::storage::MakeGrpcClient();
  std::string const object_name = "mwlid-test-grpc-" + random_suffix_ + ".txt";
  std::string const expected_content =
      "Hello from Cloud Run gRPC transport with Agent Identity!";

  std::cout << "Starting gRPC transport GCS CRUD test on bucket: "
            << bucket_name_ << ", object: " << object_name << std::endl;

  // 1. Write object
  auto writer = client.WriteObject(bucket_name_, object_name);
  writer << expected_content;
  writer.Close();
  ASSERT_THAT(writer.metadata(), IsOk());

  // 2. Read object
  auto reader = client.ReadObject(bucket_name_, object_name);
  ASSERT_TRUE(reader.good());
  std::string actual_content(std::istreambuf_iterator<char>(reader), {});
  ASSERT_THAT(actual_content, Eq(expected_content));

  // 3. Get metadata
  StatusOr<ObjectMetadata> metadata =
      client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_THAT(metadata, IsOk());
  EXPECT_THAT(metadata->name(), Eq(object_name));

  // 4. Delete object
  Status delete_status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_THAT(delete_status, IsOk());

  std::cout << "gRPC transport GCS CRUD test PASSED successfully!" << std::endl;
}

TEST_F(ManagedWorkloadIntegrationTest, RestTransportMtlsBoundCrud) {
  auto options =
      google::cloud::Options{}.set<google::cloud::storage::RestEndpointOption>(
          "https://storage.mtls.googleapis.com");
  auto client = google::cloud::storage::Client(std::move(options));
  std::string const object_name =
      "mwlid-test-rest-mtls-" + random_suffix_ + ".txt";
  std::string const expected_content =
      "Hello from Cloud Run REST transport with mTLS-bound MWLID!";

  std::cout << "Starting REST transport mTLS-bound GCS CRUD test on bucket: "
            << bucket_name_ << ", object: " << object_name << std::endl;

  // 1. Write object
  auto writer = client.WriteObject(bucket_name_, object_name);
  writer << expected_content;
  writer.Close();
  ASSERT_THAT(writer.metadata(), IsOk());

  // 2. Read object
  auto reader = client.ReadObject(bucket_name_, object_name);
  ASSERT_TRUE(reader.good());
  std::string actual_content(std::istreambuf_iterator<char>(reader), {});
  ASSERT_THAT(actual_content, Eq(expected_content));

  // 3. Get metadata
  StatusOr<ObjectMetadata> metadata =
      client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_THAT(metadata, IsOk());
  EXPECT_THAT(metadata->name(), Eq(object_name));

  // 4. Delete object
  Status delete_status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_THAT(delete_status, IsOk());

  std::cout << "REST transport mTLS-bound GCS CRUD test PASSED successfully!"
            << std::endl;
}

TEST_F(ManagedWorkloadIntegrationTest, GrpcTransportMtlsBoundCrud) {
  auto options = google::cloud::Options{}.set<google::cloud::EndpointOption>(
      "storage.mtls.googleapis.com:443");
  auto client = google::cloud::storage::MakeGrpcClient(std::move(options));
  std::string const object_name =
      "mwlid-test-grpc-mtls-" + random_suffix_ + ".txt";
  std::string const expected_content =
      "Hello from Cloud Run gRPC transport with mTLS-bound MWLID!";

  std::cout << "Starting gRPC transport mTLS-bound GCS CRUD test on bucket: "
            << bucket_name_ << ", object: " << object_name << std::endl;

  // 1. Write object
  auto writer = client.WriteObject(bucket_name_, object_name);
  writer << expected_content;
  writer.Close();
  ASSERT_THAT(writer.metadata(), IsOk());

  // 2. Read object
  auto reader = client.ReadObject(bucket_name_, object_name);
  ASSERT_TRUE(reader.good());
  std::string actual_content(std::istreambuf_iterator<char>(reader), {});
  ASSERT_THAT(actual_content, Eq(expected_content));

  // 3. Get metadata
  StatusOr<ObjectMetadata> metadata =
      client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_THAT(metadata, IsOk());
  EXPECT_THAT(metadata->name(), Eq(object_name));

  // 4. Delete object
  Status delete_status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_THAT(delete_status, IsOk());

  std::cout << "gRPC transport mTLS-bound GCS CRUD test PASSED successfully!"
            << std::endl;
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
