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

#include "google/cloud/internal/random.h"
#include "google/cloud/storage/bucket.h"
#include "google/cloud/storage/list_objects_reader.h"
#include <gmock/gmock.h>

namespace storage = google::cloud::storage;
using ::testing::HasSubstr;

namespace {
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
}  // anonymous namespace

TEST_F(BucketIntegrationTest, GetMetadata) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  auto client = storage::CreateDefaultClient();
  storage::Bucket bucket(client, bucket_name);

  auto metadata = bucket.GetMetadata();
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetaGenerationMatch_Success) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  auto client = storage::CreateDefaultClient();
  storage::Bucket bucket(client, bucket_name);

  auto metadata = bucket.GetMetadata();
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());

  auto metadata2 = bucket.GetMetadata(
      storage::Projection("noAcl"),
      storage::IfMetaGenerationMatch(metadata.metageneration()));
  EXPECT_EQ(metadata2, metadata);
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetaGenerationNotMatch_Failure) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  auto client = storage::CreateDefaultClient();
  storage::Bucket bucket(client, bucket_name);

  auto metadata = bucket.GetMetadata();
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(bucket.GetMetadata(storage::Projection("noAcl"),
                                  storage::IfMetaGenerationNotMatch(
                                      metadata.metageneration())),
               std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      bucket.GetMetadata(
          storage::Projection("noAcl"),
          storage::IfMetaGenerationNotMatch(metadata.metageneration())),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketIntegrationTest, InsertObjectMedia) {
  auto client = storage::CreateDefaultClient();
  // TODO(#681) - use random names for the object and buckets in the tests.
  auto bucket_name = BucketTestEnvironment::bucket_name();
  storage::Bucket bucket(client, bucket_name);
  auto object_name =
      std::string("the-test-object-") +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  auto metadata = bucket.InsertObject(object_name, "blah blah");
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ("storage#object", metadata.kind());
}

TEST_F(BucketIntegrationTest, InsertObjectMediaIfGenerationMatch) {
  auto client = storage::CreateDefaultClient();
  // TODO(#681) - use random names for the object and buckets in the tests.
  auto bucket_name = BucketTestEnvironment::bucket_name();
  storage::Bucket bucket(client, bucket_name);
  auto object_name =
      std::string("the-test-object-") +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  auto original = bucket.InsertObject(object_name, "blah blah",
                                      storage::IfGenerationMatch(0));
  EXPECT_EQ(bucket_name, original.bucket());
  EXPECT_EQ(object_name, original.name());
  EXPECT_EQ("storage#object", original.kind());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(bucket.InsertObject(object_name, "blah blah",
                                   storage::IfGenerationMatch(0)),
               std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(bucket.InsertObject(object_name, "blah blah",
                                                storage::IfGenerationMatch(0)),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketIntegrationTest, InsertObjectMediaIfGenerationNotMatch) {
  auto client = storage::CreateDefaultClient();
  // TODO(#681) - use random names for the object and buckets in the tests.
  auto bucket_name = BucketTestEnvironment::bucket_name();
  storage::Bucket bucket(client, bucket_name);
  auto object_name =
      std::string("the-test-object-") +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  auto original = bucket.InsertObject(object_name, "blah blah",
                                      storage::IfGenerationMatch(0));
  EXPECT_EQ(bucket_name, original.bucket());
  EXPECT_EQ(object_name, original.name());
  EXPECT_EQ("storage#object", original.kind());

  auto metadata = bucket.InsertObject(object_name, "more blah blah",
                                      storage::IfGenerationNotMatch(0));
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_NE(original.generation(), metadata.generation());
}

TEST_F(BucketIntegrationTest, ListObjects) {
  auto client = storage::CreateDefaultClient();
  auto bucket_name = BucketTestEnvironment::bucket_name();
  storage::Bucket bucket(client, bucket_name);

  auto gen = google::cloud::internal::MakeDefaultPRNG();
  auto create_small_object = [&bucket, &gen] {
    auto object_name =
        "object-" + google::cloud::internal::Sample(
                        gen, 16, "abcdefghijklmnopqrstuvwxyz0123456789");
    auto meta = bucket.InsertObject(object_name, "blah blah",
                                    storage::IfGenerationMatch(0));
    return meta.name();
  };

  std::vector<std::string> expected(3);
  std::generate_n(expected.begin(), expected.size(), create_small_object);

  storage::ListObjectsReader reader(client, bucket_name);
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

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

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
      new BucketTestEnvironment(project_id, bucket_name));

  return RUN_ALL_TESTS();
}
