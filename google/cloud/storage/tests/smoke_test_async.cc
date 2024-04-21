// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/bucket_name.h"
#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;

TEST(SmokeTest, Grpc) {
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  if (bucket_name.empty()) GTEST_SKIP();

  auto client = AsyncClient();
  auto gen = google::cloud::internal::MakeDefaultPRNG();
  auto object_name = google::cloud::storage::testing::MakeRandomObjectName(gen);

  auto insert_request = google::storage::v2::WriteObjectRequest{};
  auto& spec = *insert_request.mutable_write_object_spec();
  spec.mutable_resource()->set_bucket(BucketName(bucket_name).FullName());
  spec.mutable_resource()->set_name(object_name);
  spec.set_if_generation_match(0);
  auto insert =
      client.InsertObject(std::move(insert_request), "Hello World!").get();
  ASSERT_STATUS_OK(insert);
  auto metadata = *insert;

  auto request = google::storage::v2::ReadObjectRequest{};
  request.set_bucket(BucketName(bucket_name).FullName());
  request.set_object(metadata.name());
  request.set_generation(metadata.generation());
  auto payload = client.ReadObjectRange(std::move(request), 0, 1024).get();
  ASSERT_STATUS_OK(payload);
  std::string contents;
  for (auto v : payload->contents()) contents += std::string(v);
  EXPECT_EQ(contents, "Hello World!");

  auto delete_request = google::storage::v2::DeleteObjectRequest{};
  delete_request.set_bucket(metadata.bucket());
  delete_request.set_object(metadata.name());
  delete_request.set_generation(metadata.generation());
  auto deleted = client.DeleteObject(std::move(delete_request)).get();
  EXPECT_STATUS_OK(deleted);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
