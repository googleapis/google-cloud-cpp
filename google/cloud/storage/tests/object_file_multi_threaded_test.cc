// Copyright 2019 Google LLC
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
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>
#include <future>
#include <regex>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

auto const kObjectSize = 16 * 1024;

class ObjectFileMultiThreadedTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
    auto object_count = google::cloud::internal::GetEnv(
        "GOOGLE_CLOUD_CPP_STORAGE_TEST_OBJECT_COUNT");
    if (object_count) object_count_ = std::stoi(*object_count);
  }

  static int ThreadCount() {
    static int const kCount = [] {
      auto c = static_cast<int>(std::thread::hardware_concurrency());
      return c == 0 ? 4 : 4 * c;
    }();
    return kCount;
  }

  std::vector<std::string> CreateObjectNames() {
    std::vector<std::string> object_names(object_count_);
    // Use MakeRandomFilename() because the same name is used for
    // the destination file.
    std::generate_n(object_names.begin(), object_names.size(),
                    [this] { return MakeRandomFilename(); });
    return object_names;
  }

  Status CreateSomeObjects(Client client,
                           std::vector<std::string> const& object_names,
                           int thread_count, int modulo) {
    auto contents = [this] {
      std::unique_lock<std::mutex> lk(mu_);
      return MakeRandomData(kObjectSize);
    }();
    int index = 0;
    for (auto const& n : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto metadata =
          client.InsertObject(bucket_name_, n, contents, IfGenerationMatch(0));
      if (!metadata) return metadata.status();
    }
    return Status();
  }

  void CreateObjects(Client const& client,
                     std::vector<std::string> const& object_names) {
    // Parallelize the object creation too because it can be slow.
    int const thread_count = ThreadCount();
    auto create_some_objects = [this, &client, &object_names,
                                thread_count](int modulo) {
      return CreateSomeObjects(client, object_names, thread_count, modulo);
    };
    std::vector<std::future<Status>> tasks(thread_count);
    int modulo = 0;
    for (auto& t : tasks) {
      t = std::async(std::launch::async, create_some_objects, modulo++);
    }
    for (auto& t : tasks) {
      auto const status = t.get();
      EXPECT_STATUS_OK(status);
    }
  }

  Status DeleteSomeObjects(Client client,
                           std::vector<std::string> const& object_names,
                           int thread_count, int modulo) {
    int index = 0;
    Status status;
    for (auto const& name : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto result = client.DeleteObject(bucket_name_, name);
      if (!result.ok()) status = result;
    }
    return status;
  }

  void DeleteObjects(Client const& client,
                     std::vector<std::string> const& object_names) {
    // Parallelize the object deletion too because it can be slow.
    int const thread_count = ThreadCount();
    auto delete_some_objects = [this, &client, &object_names,
                                thread_count](int modulo) {
      return DeleteSomeObjects(client, object_names, thread_count, modulo);
    };
    std::vector<std::future<Status>> tasks(thread_count);
    int modulo = 0;
    for (auto& t : tasks) {
      t = std::async(std::launch::async, delete_some_objects, modulo++);
    }
    for (auto& t : tasks) {
      auto const status = t.get();
      EXPECT_STATUS_OK(status);
    }
  }

  std::mutex mu_;
  std::string bucket_name_;
  int object_count_ = 128;
};

TEST_F(ObjectFileMultiThreadedTest, Download) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_names = CreateObjectNames();
  std::cout << "Create test objects " << std::flush;
  ASSERT_NO_FATAL_FAILURE(CreateObjects(*client, object_names));
  std::cout << " DONE\n";

  // Create multiple threads, each downloading a portion of the objects.
  auto const thread_count = ThreadCount();
  auto download_some_objects = [this, thread_count, &client,
                                &object_names](int modulo) {
    std::cout << '+' << std::flush;
    int index = 0;
    for (auto const& name : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto status = client->DownloadToFile(bucket_name_, name, name);
      if (!status.ok()) return status;  // stop on the first error
    }
    return Status();
  };
  std::cout << "Performing downloads " << std::flush;
  std::vector<std::future<Status>> tasks(thread_count);
  int modulo = 0;
  for (auto& t : tasks) {
    t = std::async(std::launch::async, download_some_objects, modulo++);
  }
  for (auto& t : tasks) {
    auto const status = t.get();
    EXPECT_STATUS_OK(status);
  }
  std::cout << " DONE\n";

  for (auto const& name : object_names) {
    EXPECT_EQ(0, std::remove(name.c_str()));
  }

  std::cout << "Delete test objects " << std::flush;
  ASSERT_NO_FATAL_FAILURE(DeleteObjects(*client, object_names));
  std::cout << " DONE\n";
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
