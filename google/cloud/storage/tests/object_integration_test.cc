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
#include "google/cloud/storage/client.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
/// Store the project and instance captured from the command-line arguments.
class ObjectTestEnvironment : public ::testing::Environment {
 public:
  ObjectTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
};

std::string ObjectTestEnvironment::project_id_;
std::string ObjectTestEnvironment::bucket_name_;

class ObjectIntegrationTest : public ::testing::Test {
 protected:
  std::string MakeRandomObjectName() {
    return "ob-" +
           google::cloud::internal::Sample(generator_, 16,
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "012456789") +
           ".txt";
  }

  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + ObjectTestEnvironment::project_id();
  }

  std::string LoremIpsum() const {
    return R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
})""";
  }

 protected:
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};
}  // anonymous namespace

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(ObjectIntegrationTest, BasicCRUD) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();

  auto objects = client.ListObjects(bucket_name);
  std::vector<ObjectMetadata> initial_list(objects.begin(), objects.end());

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectMetadata> const& list) {
    return std::count_if(
        list.begin(), list.end(),
        [name](ObjectMetadata const& m) { return m.name() == name; });
  };

  auto object_name = MakeRandomObjectName();
  ASSERT_EQ(0, name_counter(object_name, initial_list))
      << "Test aborted. The object <" << object_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  // Create the object, but only if it does not exist already.
  ObjectMetadata insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  objects = client.ListObjects(bucket_name);

  std::vector<ObjectMetadata> current_list(objects.begin(), objects.end());
  EXPECT_EQ(1U, name_counter(object_name, current_list));

  ObjectMetadata get_meta = client.GetObjectMetadata(
      bucket_name, object_name, Generation(insert_meta.generation()));
  EXPECT_EQ(get_meta, insert_meta);

  client.DeleteObject(bucket_name, object_name);
  objects = client.ListObjects(bucket_name);
  current_list.assign(objects.begin(), objects.end());

  EXPECT_EQ(0U, name_counter(object_name, current_list));
}

TEST_F(ObjectIntegrationTest, BasicReadWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name, expected,
                                            IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, StreamingWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  auto generate_random_line = [this] {
    std::string const characters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        ".,/;:'[{]}=+-_}]`~!@#$%^&*()";
    return google::cloud::internal::Sample(generator_, 200, characters);
  };

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  for (int line = 0; line != 1000; ++line) {
    std::string random = generate_random_line() + "\n";
    os << line << ": " << random;
    expected << line << ": " << random;
  }
  ObjectMetadata meta = os.Close();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, StreamingWriteAutoClose) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::string expected = "A short string to test\n";

  {
    // Create the object, but only if it does not exist already.
    auto os =
        client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
    os << expected;
  }
  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, AccessControlCRUD) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  (void)client.InsertObject(bucket_name, object_name, LoremIpsum(),
                            IfGenerationMatch(0));

  auto entity_name = MakeEntityName();
  std::vector<ObjectAccessControl> initial_acl =
      client.ListObjectAcl(bucket_name, object_name);

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](ObjectAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_EQ(0, name_counter(entity_name, initial_acl))
      << "Test aborted. The entity <" << entity_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  ObjectAccessControl result =
      client.CreateObjectAcl(bucket_name, object_name, entity_name, "OWNER");
  EXPECT_EQ("OWNER", result.role());
  auto current_acl = client.ListObjectAcl(bucket_name, object_name);
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result.entity(), current_acl));

  auto get_result = client.GetObjectAcl(bucket_name, object_name, entity_name);
  EXPECT_EQ(get_result, result);

  ObjectAccessControl new_acl = get_result;
  new_acl.set_role("READER");
  auto updated_result =
      client.UpdateObjectAcl(bucket_name, object_name, new_acl);
  EXPECT_EQ(updated_result.role(), "READER");
  get_result = client.GetObjectAcl(bucket_name, object_name, entity_name);
  EXPECT_EQ(get_result, updated_result);

  // Remove an entity and verify it is no longer in the ACL.
  client.DeleteObjectAcl(bucket_name, object_name, entity_name);
  current_acl = client.ListObjectAcl(bucket_name, object_name);
  EXPECT_EQ(0, name_counter(result.entity(), current_acl));

  client.DeleteObject(bucket_name, object_name);
}
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
              << " <project-id> <bucket-name>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const bucket_name = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ObjectTestEnvironment(project_id,
                                                        bucket_name));

  return RUN_ALL_TESTS();
}
