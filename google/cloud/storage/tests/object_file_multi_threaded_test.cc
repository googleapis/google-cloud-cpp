// Copyright 2019 Google LLC
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
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <future>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
      return (std::max)(c / 2, 8);
    }();
    return kCount;
  }

  struct Names {
    std::string object_name;
    std::string filename;
  };

  std::vector<Names> CreateNames() {
    std::vector<Names> names(object_count_);
    std::generate_n(names.begin(), names.size(), [this] {
      return Names{MakeRandomObjectName(), MakeRandomFilename()};
    });
    return names;
  }

  Status CreateSomeObjects(Client client,
                           std::vector<Names> const& object_names,
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
      auto metadata = client.InsertObject(bucket_name_, n.object_name, contents,
                                          IfGenerationMatch(0));
      if (metadata) continue;
      // kAlreadyExists is acceptable, it happens if (1) a retry attempt
      // succeeds, but returns kUnavailable or a similar error (these can be
      // network / overload issues), (2) the next retry attempt finds the object
      // was already created.
      if (metadata.status().code() == StatusCode::kAlreadyExists) continue;
      return metadata.status();
    }
    return Status();
  }

  void CreateObjects(Client const& client, std::vector<Names> const& names) {
    // Parallelize the object creation too because it can be slow.
    int const thread_count = ThreadCount();
    auto create_some_objects = [this, &client, &names,
                                thread_count](int modulo) {
      return CreateSomeObjects(client, names, thread_count, modulo);
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
                           std::vector<Names> const& object_names,
                           int thread_count, int modulo) {
    int index = 0;
    Status status;
    for (auto const& name : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto result = client.DeleteObject(bucket_name_, name.object_name);
      if (result.ok()) continue;
      // kNotFound is acceptable, it happens if (1) a retry attempt succeeds,
      // but returns kUnavailable or a similar error, (2) the next retry attempt
      // cannot find the object.
      if (result.code() == StatusCode::kNotFound) continue;
      status = result;
    }
    return status;
  }

  void DeleteObjects(Client const& client, std::vector<Names> const& names) {
    // Parallelize the object deletion too because it can be slow.
    int const thread_count = ThreadCount();
    auto delete_some_objects = [this, &client, &names,
                                thread_count](int modulo) {
      return DeleteSomeObjects(client, names, thread_count, modulo);
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
  auto client = MakeIntegrationTestClient();
  auto const names = CreateNames();

  std::cout << "Create test objects " << std::flush;
  ASSERT_NO_FATAL_FAILURE(CreateObjects(client, names));
  std::cout << " DONE\n";

  // Create multiple threads, each downloading a portion of the objects.
  auto const thread_count = ThreadCount();
  auto download_some_objects = [this, thread_count, &client,
                                &names](int modulo) {
    std::cout << '+' << std::flush;
    int index = 0;
    for (auto const& name : names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto status =
          client.DownloadToFile(bucket_name_, name.object_name, name.filename);
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

  for (auto const& name : names) {
    EXPECT_EQ(0, std::remove(name.filename.c_str()));
  }

  std::cout << "Delete test objects " << std::flush;
  ASSERT_NO_FATAL_FAILURE(DeleteObjects(client, names));
  std::cout << " DONE\n";
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
