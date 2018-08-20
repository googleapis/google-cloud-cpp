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
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
using ::testing::HasSubstr;
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
  ObjectMetadata insert_meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection("full"));
  objects = client.ListObjects(bucket_name);

  std::vector<ObjectMetadata> current_list(objects.begin(), objects.end());
  EXPECT_EQ(1U, name_counter(object_name, current_list));

  ObjectMetadata get_meta = client.GetObjectMetadata(
      bucket_name, object_name, Generation(insert_meta.generation()),
      Projection("full"));
  EXPECT_EQ(get_meta, insert_meta);

  ObjectMetadata update = get_meta;
  update.mutable_acl().emplace_back(
      ObjectAccessControl().set_role("READER").set_entity(
          "allAuthenticatedUsers"));
  update.set_cache_control("no-cache")
      .set_content_disposition("inline")
      .set_content_encoding("identity")
      .set_content_language("en")
      .set_content_type("plain/text");
  update.mutable_metadata().emplace("updated", "true");
  ObjectMetadata updated_meta =
      client.UpdateObject(bucket_name, object_name, update, Projection("full"));

  // Because some of the ACL values are not predictable we convert the values we
  // care about to strings and compare that.
  {
    auto acl_to_string_vector =
        [](std::vector<ObjectAccessControl> const& acl) {
          std::vector<std::string> v;
          std::transform(acl.begin(), acl.end(), std::back_inserter(v),
                         [](ObjectAccessControl const& x) {
                           return x.entity() + " = " + x.role();
                         });
          return v;
        };
    auto expected = acl_to_string_vector(update.acl());
    auto actual = acl_to_string_vector(updated_meta.acl());
    EXPECT_THAT(expected, ::testing::UnorderedElementsAreArray(actual));
  }
  EXPECT_EQ(update.cache_control(), updated_meta.cache_control())
      << updated_meta;
  EXPECT_EQ(update.content_disposition(), updated_meta.content_disposition())
      << updated_meta;
  EXPECT_EQ(update.content_encoding(), updated_meta.content_encoding())
      << updated_meta;
  EXPECT_EQ(update.content_language(), updated_meta.content_language())
      << updated_meta;
  EXPECT_EQ(update.content_type(), updated_meta.content_type()) << updated_meta;
  EXPECT_EQ(update.metadata(), updated_meta.metadata()) << updated_meta;

  ObjectMetadata desired_patch = updated_meta;
  desired_patch.set_content_language("en");
  desired_patch.mutable_metadata().erase("updated");
  desired_patch.mutable_metadata().emplace("patched", "true");
  ObjectMetadata patched_meta =
      client.PatchObject(bucket_name, object_name, updated_meta, desired_patch);
  EXPECT_EQ(desired_patch.metadata(), patched_meta.metadata()) << patched_meta;
  EXPECT_EQ(desired_patch.content_language(), patched_meta.content_language())
      << patched_meta;

  client.DeleteObject(bucket_name, object_name);
  objects = client.ListObjects(bucket_name);
  current_list.assign(objects.begin(), objects.end());

  EXPECT_EQ(0U, name_counter(object_name, current_list));
}

TEST_F(ObjectIntegrationTest, FullPatch) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  // Create the object, but only if it does not exist already.
  ObjectMetadata original =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection("full"));

  ObjectMetadata desired = original;
  desired.mutable_acl().push_back(ObjectAccessControl()
                                      .set_entity("allAuthenticatedUsers")
                                      .set_role("READER"));
  if (original.cache_control() != "no-cache") {
    desired.set_cache_control("no-cache");
  } else {
    desired.set_cache_control("");
  }
  if (original.content_disposition() != "inline") {
    desired.set_content_disposition("inline");
  } else {
    desired.set_content_disposition("attachment; filename=test.txt");
  }
  if (original.content_encoding() != "identity") {
    desired.set_content_encoding("identity");
  } else {
    desired.set_content_encoding("");
  }
  // Use 'en' and 'fr' as test languages because they are known to be supported.
  // The server rejects private tags such as 'x-pig-latin'.
  if (original.content_language() != "en") {
    desired.set_content_language("en");
  } else {
    desired.set_content_language("fr");
  }
  if (original.content_type() != "application/octet-stream") {
    desired.set_content_type("application/octet-stream");
  } else {
    desired.set_content_type("application/text");
  }

  if (original.has_metadata("test-label")) {
    original.mutable_metadata().erase("test-label");
  } else {
    original.mutable_metadata().emplace("test-label", "test-value");
  }

  ObjectMetadata patched =
      client.PatchObject(bucket_name, object_name, original, desired);

  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_EQ(1U, std::count_if(patched.acl().begin(), patched.acl().end(),
                              [](ObjectAccessControl const& x) {
                                return x.entity() == "allAuthenticatedUsers";
                              }));

  EXPECT_EQ(desired.cache_control(), patched.cache_control());
  EXPECT_EQ(desired.content_disposition(), patched.content_disposition());
  EXPECT_EQ(desired.content_encoding(), patched.content_encoding());
  EXPECT_EQ(desired.content_language(), patched.content_language());
  EXPECT_EQ(desired.content_type(), patched.content_type());
  EXPECT_EQ(desired.metadata(), patched.metadata());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ListObjectsVersions) {
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  Client client;

  // This test requires the bucket to be configured with versioning. The buckets
  // used by the CI build are already configured with versioning enabled. The
  // bucket created in the testbench also has versioning. Regardless, check here
  // first to produce a better error message if there is a configuration
  // problem.
  auto bucket_meta = client.GetBucketMetadata(bucket_name);
  ASSERT_TRUE(bucket_meta.versioning().has_value());
  ASSERT_TRUE(bucket_meta.versioning().value().enabled);

  auto create_object_with_3_versions = [&client, &bucket_name, this] {
    auto object_name = MakeRandomObjectName();
    auto meta = client.InsertObject(bucket_name, object_name,
                                    "contents for the first revision",
                                    storage::IfGenerationMatch(0));
    client.InsertObject(bucket_name, object_name,
                        "contents for the second revision");
    client.InsertObject(bucket_name, object_name,
                        "contents for the final revision");
    return meta.name();
  };

  std::vector<std::string> expected(4);
  std::generate_n(expected.begin(), expected.size(),
                  create_object_with_3_versions);

  ListObjectsReader reader = client.ListObjects(bucket_name, Versions(true));
  std::vector<std::string> actual;
  for (auto it = reader.begin(); it != reader.end(); ++it) {
    auto const& meta = *it;
    EXPECT_EQ(bucket_name, meta.bucket());
    actual.push_back(meta.name());
  }
  auto produce_joined_list = [&actual] {
    std::string joined;
    for (auto const& x : actual) {
      joined += "  ";
      joined += x;
      joined += "\n";
    }
    return joined;
  };
  // There may be a lot of other objects in the bucket, so we want to verify
  // that any objects we created are found there, but cannot expect a perfect
  // match.

  for (auto const& name : expected) {
    EXPECT_EQ(3, std::count(actual.begin(), actual.end(), name))
        << "Expected to find 3 copies of " << name << " in the object list:\n"
        << produce_joined_list();
  }
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

TEST_F(ObjectIntegrationTest, EncryptedReadWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create a pseudo-random number generator, initialized using the default
  // entropy source. WARNING: this PRNG has not gone through a security audit,
  // it is possible that the random numbers are sufficiently predictable to make
  // them unusable for security purposes.  Application developers should consult
  // with their security team before relying on this (or any other) source for
  // encryption keys.
  auto generator = google::cloud::internal::MakeDefaultPRNG();

  // Applications should save the key in a secure location after creating them,
  // Google Cloud Storage does not save customer-supplied keys, and if lost the
  // encrypted data cannot be decrypted.
  EncryptionKeyData key = CreateKeyFromGenerator(generator);

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, expected,
                          IfGenerationMatch(0), EncryptionKey(key));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  ASSERT_TRUE(meta.has_customer_encryption());
  EXPECT_EQ("AES256", meta.customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta.customer_encryption().key_sha256);

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name, EncryptionKey(key));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ReadNotFound) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  EXPECT_TRUE(stream.eof());
  EXPECT_FALSE(stream.IsOpen());
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

  new_acl = get_result;
  new_acl.set_role("OWNER");
  get_result =
      client.PatchObjectAcl(bucket_name, object_name, entity_name, get_result,
                            new_acl, IfMatchEtag(get_result.etag()));
  EXPECT_EQ(get_result.role(), new_acl.role());

  // Remove an entity and verify it is no longer in the ACL.
  client.DeleteObjectAcl(bucket_name, object_name, entity_name);
  current_acl = client.ListObjectAcl(bucket_name, object_name);
  EXPECT_EQ(0, name_counter(result.entity(), current_acl));

  client.DeleteObject(bucket_name, object_name);
}

class CaptureSendHeaderBackend : public LogBackend {
 public:
  std::vector<std::string> log_lines;

  void Process(LogRecord const& lr) override {
    // Break the records in lines, because we will analyze the output per line.
    std::istringstream is(lr.message);
    while (not is.eof()) {
      std::string line;
      std::getline(is, line);
      log_lines.emplace_back(std::move(line));
    }
  }

  void ProcessWithOwnership(LogRecord lr) override { Process(lr); }
};

/**
 * @test Verify that `QuotaUser` inserts the correct query parameter.
 *
 * Testing for `QuotaUser` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_F(ObjectIntegrationTest, InsertWithQuotaUser) {
  auto backend = std::make_shared<CaptureSendHeaderBackend>();

  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto id = LogSink::Instance().AddBackend(backend);
  ObjectMetadata insert_meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), QuotaUser("test-quota-user"));
  LogSink::Instance().RemoveBackend(id);

  // Create the regular expression we want to match.
  std::regex re = [&bucket_name] {
    std::string regex = ".* POST .*";
    regex += "/b/" + bucket_name + "/o";
    regex += ".*quotaUser=test-quota-user.*";
    return std::regex(regex, std::regex_constants::egrep);
  }();

  long count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [&re](std::string const& line) { return std::regex_match(line, re); });
  EXPECT_LT(0, count);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, InsertWithContentType) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), ContentType("text/plain"));
  EXPECT_EQ("text/plain", meta.content_type());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, WriteWithContentType) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               ContentType("text/plain"));
  os << LoremIpsum();
  ObjectMetadata meta = os.Close();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());

  client.DeleteObject(bucket_name, object_name);
}

}  // anonymous namespace
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
