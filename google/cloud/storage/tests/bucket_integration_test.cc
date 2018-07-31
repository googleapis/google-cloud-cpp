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
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::HasSubstr;

/// Store the project and instance captured from the command-line arguments.
class BucketTestEnvironment : public ::testing::Environment {
 public:
  BucketTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
};

std::string BucketTestEnvironment::project_id_;
std::string BucketTestEnvironment::bucket_name_;

class BucketIntegrationTest : public ::testing::Test {};

TEST_F(BucketIntegrationTest, BasicCRUD) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  auto project_id = BucketTestEnvironment::project_id();
  Client client;

  auto buckets = client.ListBucketsForProject(project_id);
  std::vector<BucketMetadata> initial_buckets(buckets.begin(), buckets.end());
  // Since `bucket_name` should be available, we do not expect this list to be
  // empty.
  EXPECT_FALSE(initial_buckets.empty())
      << "Unexpected empty list with project_id=" << project_id
      << ", bucket_name=" << bucket_name;

  auto name_counter = [](std::string const& name,
                         std::vector<BucketMetadata> const& list) {
    return std::count_if(
        list.begin(), list.end(),
        [&name](BucketMetadata const& m) { return m.name() == name; });
  };
  EXPECT_EQ(1U, name_counter(bucket_name, initial_buckets));
}

TEST_F(BucketIntegrationTest, GetMetadata) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetaGenerationMatch_Success) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());

  auto metadata2 = client.GetBucketMetadata(
      bucket_name, storage::Projection("noAcl"),
      storage::IfMetaGenerationMatch(metadata.metageneration()));
  EXPECT_EQ(metadata2, metadata);
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetaGenerationNotMatch_Failure) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      client.GetBucketMetadata(
          bucket_name, storage::Projection("noAcl"),
          storage::IfMetaGenerationNotMatch(metadata.metageneration())),
      std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.GetBucketMetadata(
          bucket_name, storage::Projection("noAcl"),
          storage::IfMetaGenerationNotMatch(metadata.metageneration())),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketIntegrationTest, InsertObjectMedia) {
  // TODO(#681) - use random names for the object and buckets in the tests.
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;
  auto object_name =
      std::string("the-test-object-") +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  auto metadata = client.InsertObject(bucket_name, object_name, "blah blah");
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ("storage#object", metadata.kind());
}

TEST_F(BucketIntegrationTest, InsertObjectMediaIfGenerationMatch) {
  // TODO(#681) - use random names for the object and buckets in the tests.
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;
  auto object_name =
      std::string("the-test-object-") +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  auto original = client.InsertObject(bucket_name, object_name, "blah blah",
                                      storage::IfGenerationMatch(0));
  EXPECT_EQ(bucket_name, original.bucket());
  EXPECT_EQ(object_name, original.name());
  EXPECT_EQ("storage#object", original.kind());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(client.InsertObject(bucket_name, object_name, "blah blah",
                                   storage::IfGenerationMatch(0)),
               std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.InsertObject(bucket_name, object_name, "blah blah",
                          storage::IfGenerationMatch(0)),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketIntegrationTest, InsertObjectMediaIfGenerationNotMatch) {
  // TODO(#681) - use random names for the object and buckets in the tests.
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;
  auto object_name =
      std::string("the-test-object-") +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  auto original = client.InsertObject(bucket_name, object_name, "blah blah",
                                      storage::IfGenerationMatch(0));
  EXPECT_EQ(bucket_name, original.bucket());
  EXPECT_EQ(object_name, original.name());
  EXPECT_EQ("storage#object", original.kind());

  auto metadata =
      client.InsertObject(bucket_name, object_name, "more blah blah",
                          storage::IfGenerationNotMatch(0));
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_NE(original.generation(), metadata.generation());
}

TEST_F(BucketIntegrationTest, ListObjects) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto gen = google::cloud::internal::MakeDefaultPRNG();
  auto create_small_object = [&client, &bucket_name, &gen] {
    auto object_name =
        "object-" + google::cloud::internal::Sample(
                        gen, 16, "abcdefghijklmnopqrstuvwxyz0123456789");
    auto meta = client.InsertObject(bucket_name, object_name, "blah blah",
                                    storage::IfGenerationMatch(0));
    return meta.name();
  };

  std::vector<std::string> expected(3);
  std::generate_n(expected.begin(), expected.size(), create_small_object);

  ListObjectsReader reader = client.ListObjects(bucket_name);
  std::vector<std::string> actual;
  for (auto it = reader.begin(); it != reader.end(); ++it) {
    auto const& meta = *it;
    EXPECT_EQ(bucket_name, meta.bucket());
    actual.push_back(meta.name());
  }
  // There may be a lot of other objects in the bucket, so we want to verify
  // that any objects we created are found there, but cannot expect a perfect
  // match.
  for (auto const& name : expected) {
    EXPECT_EQ(1, std::count(actual.begin(), actual.end(), name));
  }
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <bucket>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const bucket_name = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::BucketTestEnvironment(project_id,
                                                        bucket_name));

  return RUN_ALL_TESTS();
}
