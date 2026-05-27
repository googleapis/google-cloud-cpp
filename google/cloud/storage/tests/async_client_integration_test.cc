// Copyright 2022 Google LLC
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
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/async/read_all.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <chrono>
#include "google/cloud/storage/async/options.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <queue>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Not;
using ::testing::Optional;
using ::testing::VariantWith;

class AsyncClientIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
protected:

  using google::cloud::storage::testing::StorageIntegrationTest::
      ScheduleForDelete;
  void ScheduleForDelete(google::storage::v2::Object const& object) {
    ScheduleForDelete(storage::ObjectMetadata{}
                          .set_bucket(MakeBucketName(object.bucket())->name())
                          .set_name(object.name())
                          .set_generation(object.generation()));
  }

private:
  std::string bucket_name_;
};

namespace gcs = ::google::cloud::storage;

// auto AlwaysRetry() {
//   return google::cloud::Options{}.set<google::cloud::storage::IdempotencyPolicyOption>(
//       MakeAlwaysRetryIdempotencyPolicy);
// }

google::cloud::Options MakeOptions(google::cloud::Options opts) {
  auto fallback = google::cloud::Options{};
  if (auto v = google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_GRPC_ENDPOINT")) {
    fallback.set<google::cloud::EndpointOption>(*v);
  }
  if (auto v = google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_JSON_ENDPOINT")) {
    fallback.set<google::cloud::storage::RestEndpointOption>(*v);
  }
  if (auto v = google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_AUTHORITY")) {
    fallback.set<google::cloud::AuthorityOption>(*v);
  }
  if (auto v = google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_TARGET_API_VERSION")) {
    fallback.set<google::cloud::storage::internal::TargetApiVersionOption>(*v);
  }
  fallback.set<google::cloud::storage_experimental::EnableGrpcMetricsOption>(false);
  return google::cloud::internal::MergeOptions(std::move(opts), fallback);
}


google::cloud::storage::Client MakeGrpcClient(std::string project_id) {
  auto options = MakeOptions(google::cloud::Options{}
                      .set<gcs::ProjectIdOption>(project_id));
  return google::cloud::storage::MakeGrpcClient(std::move(options));
}

google::cloud::storage::AsyncClient MakeAsyncClient(std::string project_id) {
  auto options = MakeOptions(google::cloud::Options{}
                          .set<gcs::ProjectIdOption>(project_id)
                          .set<google::cloud::LoggingComponentsOption>({"rpc"})
                          .set<google::cloud::OpenTelemetryTracingOption>(true));
  return google::cloud::storage::AsyncClient(options);
}

class ThreadPool {
public:
    // Constructor initializes the thread pool with a given number of worker threads.
    ThreadPool(size_t threads) : stop_(false) {
        if (threads == 0) {
            throw std::invalid_argument("Thread count cannot be zero.");
        }
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        // Acquire a lock on the task queue.
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        
                        // Wait for a task to be available or for the pool to stop.
                        this->condition_.wait(lock, [this] {
                            return this->stop_ || !this->tasks_.empty();
                        });

                        // If the pool is stopping and no tasks are left, exit the thread.
                        if (this->stop_ && this->tasks_.empty()) {
                            return;
                        }
                        
                        // Get the next task from the queue.
                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    
                    // Execute the task.
                    task();
                }
            });
        }
    }

    // Adds a new task to the thread pool.
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;

        // Create a packaged_task to wrap the function and its arguments.
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            // Acquire a lock on the queue and push the task.
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Don't allow enqueueing after stopping the pool.
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        // Notify one waiting thread that a new task is available.
        condition_.notify_one();
        return res;
    }

    // Destructor stops all worker threads and joins them.
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        // Notify all threads so they can wake up and exit their loops.
        condition_.notify_all();
        
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    
    bool stop_;
};


// void ReadRangeTask(std::shared_ptr<ObjectDescriptor>  descriptor,
//                    std::int64_t& offset,
//                    std::int64_t& limit) {
//     AsyncReader r;
//     AsyncToken t;
//     
//     // 1. Get the reader and token for the specified range
//     // MODIFIED: Call Read() directly on the descriptor object
//     std::tie(r, t) = descriptor->Read(offset, limit);
//     
//     // 2. Consume the entire stream for this range
//     while (t.valid()) {
//         auto read = r.Read(std::move(t)).get();
//         
//         // ASSERT_STATUS_OK will flag the test as failed and abort this
//         // thread if the status is not OK.
//         ASSERT_STATUS_OK(read); 
//         
//         ReadPayload p;
//         AsyncToken t_new;
//         std::tie(p, t_new) = *std::move(read);
//         t = std::move(t_new);
//         
//         // In this test, we are discarding the payload `p`, just as
//         // the original single-threaded loop did.
//     }
// }


TEST_F(AsyncClientIntegrationTest, StartAppendableUploadEmpty) {
  auto project_id = "bajajnehaa-devrel-test";
  // auto const kproject = google::cloud::Project(project_id);

  auto client = MakeGrpcClient(project_id);

  auto bucket_name = std::string{"gcs-grpc-team-fastbyte-bajajnehaa-test-us-west4"};
  auto object_name = MakeRandomObjectName();
  // auto placement = gcs::BucketCustomPlacementConfig{{"us-west4-a"}};
  // auto hns = gcs::BucketHierarchicalNamespace{true};
  // auto ubla = gcs::BucketIamConfiguration{gcs::UniformBucketLevelAccess{true, {}}, absl::nullopt};

  auto constexpr kBlockSize = 1024;
  auto constexpr kBlockCount = 1;
  auto const block = MakeRandomData(kBlockSize);
  // auto const block2 = MakeRandomData(kBlockSize);

  auto async = MakeAsyncClient(project_id);
  auto w = async.StartAppendableObjectUpload(BucketName(bucket_name), object_name)
                .get();
  ASSERT_STATUS_OK(w);

  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  for (int i = 0; i < kBlockCount; ++i) {
    std::cout << "Writing data iteration #" << i << std::endl;
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }
  
  auto metadata1 = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata1);
  std::cout << "Request metadata: " << metadata1->generation() << std::endl;

  auto close = writer.Close().get();
  ASSERT_STATUS_OK(close);

  // Read object using Open() and calculate latency

  // 1. With Checksums Enabled
  {
    auto start = std::chrono::steady_clock::now();
    auto descriptor_status = async.Open(BucketName(bucket_name), object_name,
                                        Options{}
                                        .set<EnableCrc32cValidationOption>(true)
                                        .set<EnableMD5ValidationOption>(true)).get();
    ASSERT_STATUS_OK(descriptor_status);
    auto descriptor = *std::move(descriptor_status);
    
    auto [r, t] = descriptor.Read(0, kBlockSize);
    while (t.valid()) {
      auto read = r.Read(std::move(t)).get();
      ASSERT_STATUS_OK(read);
      auto [p, t_new] = *std::move(read);
      t = std::move(t_new);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Read with checksums enabled latency: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms" << std::endl;
  }

  // 2. With Checksums Disabled
  {
    auto start = std::chrono::steady_clock::now();
    auto descriptor_status = async.Open(BucketName(bucket_name), object_name,
                                        Options{}
                                        .set<EnableCrc32cValidationOption>(false)
                                        .set<EnableMD5ValidationOption>(false)).get();
    ASSERT_STATUS_OK(descriptor_status);
    auto descriptor = *std::move(descriptor_status);
    
    auto [r, t] = descriptor.Read(0, kBlockSize);
    while (t.valid()) {
      auto read = r.Read(std::move(t)).get();
      ASSERT_STATUS_OK(read);
      auto [p, t_new] = *std::move(read);
      t = std::move(t_new);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Read with checksums disabled latency: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms" << std::endl;
  }

  // Clean up
  auto delete_status = client.DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(delete_status);
}


}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
