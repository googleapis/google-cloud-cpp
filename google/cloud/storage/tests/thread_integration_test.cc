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
#include <future>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
/// Store the project and instance captured from the command-line arguments.
class ThreadTestEnvironment : public ::testing::Environment {
 public:
  ThreadTestEnvironment(std::string project, std::string location) {
    project_id_ = std::move(project);
    location_ = std::move(location);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& location() { return location_; }

 private:
  static std::string project_id_;
  static std::string location_;
};

std::string ThreadTestEnvironment::project_id_;
std::string ThreadTestEnvironment::location_;

class ThreadIntegrationTest : public ::testing::Test {
 protected:
  std::string MakeRandomBucketName() {
    // The total length of this bucket name must be <= 63 characters,
    static std::string const prefix = "gcs-cpp-test-bucket-";
    static std::size_t const kMaxBucketNameLength = 63;
    std::size_t const max_random_characters =
        kMaxBucketNameLength - prefix.size();
    return prefix + google::cloud::internal::Sample(
                        generator_, static_cast<int>(max_random_characters),
                        "abcdefghijklmnopqrstuvwxyz012456789");
  }

  std::string MakeRandomObjectName() {
    return google::cloud::internal::Sample(generator_, 24,
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "012456789") +
           ".txt";
  }

  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + ThreadTestEnvironment::project_id();
  }

 protected:
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

std::string LoremIpsum() {
  return R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
})""";
}

using ObjectNameList = std::vector<std::string>;

/**
 * Divide @p source in to @p count groups of approximately equal size.
 */
std::vector<ObjectNameList> DivideIntoEqualSizedGroups(
    ObjectNameList const& source, int count) {
  std::vector<ObjectNameList> groups;
  groups.reserve(count);
  auto div = source.size() / count;
  auto rem = source.size() % count;
  // begin points to the beginning of the next chunk, it is incremented inside
  // the loop by the number of elements.
  std::size_t size;
  for (auto begin = source.begin(); begin != source.end(); begin += size) {
    size = div;
    if (rem != 0) {
      size++;
      rem--;
    }
    auto remaining =
        static_cast<std::size_t>(std::distance(begin, source.end()));
    if (remaining < size) {
      size = remaining;
    }
    auto end = begin;
    std::advance(end, size);
    groups.emplace_back(ObjectNameList(begin, end));
  }
  return groups;
}

void CreateObjects(std::string const& bucket_name, ObjectNameList group) {
  // Create our own client so no state is shared with the other threads.
  Client client;
  for (auto const& object_name : group) {
    (void)client.InsertObject(bucket_name, object_name, LoremIpsum(),
                              IfGenerationMatch(0));
  }
}

void DeleteObjects(std::string const& bucket_name, ObjectNameList group) {
  // Create our own client so no state is shared with the other threads.
  Client client;
  for (auto const& object_name : group) {
    (void)client.DeleteObject(bucket_name, object_name);
  }
}
}  // anonymous namespace

TEST_F(ThreadIntegrationTest, Unshared) {
  auto project_id = ThreadTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  Client client;

  auto bucket = client.CreateBucketForProject(
      bucket_name, project_id,
      BucketMetadata()
          .set_storage_class(storage_class::Regional())
          .set_location(ThreadTestEnvironment::location())
          .disable_versioning(),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  EXPECT_EQ(bucket_name, bucket.name());

  constexpr int kObjectCount = 10000;
  std::vector<std::string> objects;
  objects.reserve(kObjectCount);
  std::generate_n(std::back_inserter(objects), kObjectCount,
                  [this] { return MakeRandomObjectName(); });

  unsigned int thread_count = std::thread::hardware_concurrency();
  if (thread_count == 0) {
    thread_count = 4;
  }

  std::cout << "Creating " << kObjectCount << " objects " << std::flush;
  auto groups = DivideIntoEqualSizedGroups(objects, thread_count);
  std::vector<std::future<void>> tasks;
  for (auto const& g : groups) {
    tasks.emplace_back(
        std::async(std::launch::async, &CreateObjects, bucket_name, g));
  }
  for (auto& t : tasks) {
    t.get();
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE" << std::endl;

  std::cout << "Deleting " << kObjectCount << " objects " << std::flush;
  tasks.clear();
  for (auto const& g : groups) {
    tasks.emplace_back(
        std::async(std::launch::async, &DeleteObjects, bucket_name, g));
  }
  for (auto& t : tasks) {
    t.get();
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE" << std::endl;

  std::cout << "Deleting " << bucket_name << std::endl;
  client.DeleteBucket(bucket_name);
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
              << " <project-id> <location (GCP region, e.g us-east1)>"
              << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const location = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ThreadTestEnvironment(project_id, location));

  return RUN_ALL_TESTS();
}
