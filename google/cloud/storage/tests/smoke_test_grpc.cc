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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;

TEST(SmokeTest, Grpc) {
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  if (bucket_name.empty()) GTEST_SKIP();

  auto client = MakeGrpcClient();
  auto gen = google::cloud::internal::MakeDefaultPRNG();
  auto object_name = google::cloud::storage::testing::MakeRandomObjectName(gen);

  auto writer =
      client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  writer << "Hello World!";
  writer.Close();
  ASSERT_STATUS_OK(writer.metadata());
  auto metadata = *writer.metadata();

  auto reader = client.ReadObject(bucket_name, metadata.name(),
                                  Generation(metadata.generation()));
  std::string contents{std::istreambuf_iterator<char>{reader}, {}};
  EXPECT_FALSE(reader.bad());
  EXPECT_EQ(contents, "Hello World!");
  auto deleted = client.DeleteObject(metadata.bucket(), metadata.name(),
                                     Generation(metadata.generation()));
  EXPECT_STATUS_OK(deleted);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
