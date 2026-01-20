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
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <queue>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
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
//   return
//   google::cloud::Options{}.set<google::cloud::storage_experimental::IdempotencyPolicyOption>(
//       MakeAlwaysRetryIdempotencyPolicy);
// }

google::cloud::Options MakeOptions(google::cloud::Options opts) {
  auto fallback = google::cloud::Options{};
  if (auto v = google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_GRPC_ENDPOINT")) {
    fallback.set<google::cloud::EndpointOption>(*v);
  }
  if (auto v = google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_JSON_ENDPOINT")) {
    fallback.set<google::cloud::storage::RestEndpointOption>(*v);
  }
  if (auto v = google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_AUTHORITY")) {
    fallback.set<google::cloud::AuthorityOption>(*v);
  }
  if (auto v = google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_TARGET_API_VERSION")) {
    fallback.set<google::cloud::storage::internal::TargetApiVersionOption>(*v);
  }
  fallback.set<google::cloud::storage_experimental::EnableGrpcMetricsOption>(
      false);
  return google::cloud::internal::MergeOptions(std::move(opts), fallback);
}

google::cloud::storage::Client MakeGrpcClient(std::string project_id) {
  auto options = MakeOptions(
      google::cloud::Options{}.set<gcs::ProjectIdOption>(project_id));
  return google::cloud::storage::MakeGrpcClient(std::move(options));
}

google::cloud::storage_experimental::AsyncClient MakeAsyncClient(
    std::string project_id) {
  auto options =
      MakeOptions(google::cloud::Options{}
                      .set<gcs::ProjectIdOption>(project_id)
                      .set<google::cloud::LoggingComponentsOption>({"rpc"})
                      .set<google::cloud::OpenTelemetryTracingOption>(true));
  return google::cloud::storage_experimental::AsyncClient(options);
}

class ThreadPool {
 public:
  // Constructor initializes the thread pool with a given number of worker
  // threads.
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
            this->condition_.wait(
                lock, [this] { return this->stop_ || !this->tasks_.empty(); });

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
  auto enqueue(F&& f, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    // Create a packaged_task to wrap the function and its arguments.
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

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

void ReadRangeTask(std::shared_ptr<ObjectDescriptor> descriptor,
                   std::int64_t& offset, std::int64_t& limit) {
  AsyncReader r;
  AsyncToken t;

  // 1. Get the reader and token for the specified range
  // MODIFIED: Call Read() directly on the descriptor object
  std::tie(r, t) = descriptor->Read(offset, limit);

  // 2. Consume the entire stream for this range
  while (t.valid()) {
    auto read = r.Read(std::move(t)).get();

    // ASSERT_STATUS_OK will flag the test as failed and abort this
    // thread if the status is not OK.
    ASSERT_STATUS_OK(read);

    ReadPayload p;
    AsyncToken t_new;
    std::tie(p, t_new) = *std::move(read);
    t = std::move(t_new);

    // In this test, we are discarding the payload `p`, just as
    // the original single-threaded loop did.
  }
}

TEST_F(AsyncClientIntegrationTest, StartAppendableUploadEmpty) {
  auto project_id = "bajajnehaa-devrel-test";
  // auto const kproject = google::cloud::Project(project_id);

  auto client = MakeGrpcClient(project_id);

  auto bucket_name =
      std::string{"gcs-grpc-team-fastbyte-bajajnehaa-test-us-west4"};
  auto object_name = "vaibhav-test-file-111";
  auto placement = gcs::BucketCustomPlacementConfig{{"us-west4-a"}};
  // auto hns = gcs::BucketHierarchicalNamespace{true};
  auto ubla = gcs::BucketIamConfiguration{
      gcs::UniformBucketLevelAccess{true, {}}, absl::nullopt};

  auto constexpr kBlockSize = 1024;
  auto constexpr kBlockCount = 10000000;  //
  auto const block = MakeRandomData(kBlockSize);
  auto const block2 = MakeRandomData(kBlockSize);

  auto async = MakeAsyncClient(project_id);
  // auto w = async.StartBufferedUpload(BucketName(bucket_name), object_name)
  //               .get();
  // ASSERT_STATUS_OK(w);
  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name), object_name)
          .get();
  ASSERT_STATUS_OK(w);

  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  for (int i = 0; i < kBlockCount; ++i) {
    // std::cout << "Writing data iteration #" << i << std::endl;
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  auto metadata1 = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata1);
  //   std::cout << "Request metadata: " << metadata1->generation() <<
  //   std::endl;
  EXPECT_EQ(1, 2);

  // auto close = writer.Close();

  // auto object_metadata = client.GetObjectMetadata(bucket_name, object_name);
  // auto m = *object_metadata;
  // auto generation = m.generation();

  // auto w1 = async.ResumeAppendableObjectUpload(BucketName(bucket_name),
  // object_name, generation)
  //               .get();

  // ASSERT_STATUS_OK(w1);

  // AsyncWriter writer1;
  // AsyncToken token1;
  // std::tie(writer1, token1) = *std::move(w1);

  // for (int i = 0; i < kBlockCount; ++i) {
  //   // std::cout << "Writing data iteration #" << i << std::endl;
  //   auto p = writer1.Write(std::move(token1), WritePayload(block)).get();
  //   ASSERT_STATUS_OK(p);
  //   token1 = *std::move(p);
  // }

  // // auto object_metadata1 = client.GetObjectMetadata(bucket_name,
  // object_name);
  // // auto m1 = *object_metadata1;
  // // // auto generation1 = m1.generation();
  // // std::cout << "Object metadata1: " << m << std::endl;

  // auto metadata = writer1.Finalize(std::move(token1)).get();
  // ASSERT_STATUS_OK(metadata);
  // // // ScheduleForDelete(*metadata);

  // EXPECT_EQ(metadata->bucket(), BucketName(bucket_name).FullName());
  // EXPECT_EQ(metadata->name()," object_name");
  // EXPECT_EQ(metadata->size(), kBlockCount * kBlockSize);
  // EXPECT_EQ("dddd", "Sdfs");
  // std::cout << "Test completed successfully" << std::endl;
  // client.DeleteObject(bucket_name, object_name);

  //   auto spec = google::storage::v2::BidiReadObjectSpec{};
  //   // std::cout << object_metadata->bucket() << "\n";

  // //
  // spec.set_bucket("projects/_/buckets/gcs-grpc-team-fastbyte-bajajnehaa-test-us-west4");
  // //   spec.set_object(object_name);
  // //   auto descriptor_status = async.Open(spec).get();
  // //   ASSERT_STATUS_OK(descriptor_status);
  // //   ObjectDescriptor descriptor = *std::move(descriptor_status);
  // //   auto descriptor_ptr =
  // //       std::make_shared<ObjectDescriptor>(std::move(descriptor));
  //   // std::shared_ptr<ObjectDescriptor> descriptor_ptr =
  //   std::make_shared<ObjectDescriptor>(*descriptor);

  //   // --- Start of ThreadPool implementation ---

  //   // 1. Initialize the ThreadPool
  //   // Use hardware_concurrency to get a reasonable number of threads
  //   size_t num_threads = std::thread::hardware_concurrency();
  //   std::cout << "Starting ThreadPool with " << num_threads << " threads." <<
  //   std::endl; ThreadPool pool(num_threads);

  //   // 2. Define read parameters and storage for futures
  //   std::vector<std::future<void>> futures;
  //   int num_reads = 1000;
  //   std::int64_t read_offset = 0;
  //   std::int64_t read_limit = 1024 * 1024 * 1024; // 1 GiB

  //   std::cout << "Enqueuing " << num_reads << " read tasks..." << std::endl;

  //   // 3. Enqueue all the read tasks
  //   // The original loop is replaced with this loop.
  //   for (int i = 0; i < num_reads; ++i) {
  //       // Pass *descriptor (the ObjectDescriptor) by value.
  //       // This is safe because it's a copyable wrapper.
  //       futures.push_back(
  //           pool.enqueue(ReadRangeTask, descriptor_ptr, read_offset,
  //           read_limit)
  //       );
  //   }

  //   // 4. Wait for all enqueued tasks to complete
  //   std::cout << "Waiting for all " << futures.size() << " read tasks to
  //   complete..." << std::endl; for (auto& f : futures) {
  //       f.get(); // This blocks until the future is ready.
  //                // If a task failed (e.g., via ASSERT_STATUS_OK),
  //                // gtest will have already flagged the failure.
  //                // If the task threw an exception, get() will re-throw it.
  //   }

  //   std::cout << "All " << num_reads << " parallel read tasks completed." <<
  //   std::endl;

  // --- End of ThreadPool implementation ---

  // auto actual0 = std::string{};
  // for(int i =0 ; i< 1000 ; i++){
  //   std::tie(r0, t0) = descriptor->Read(0 , 1024* 1024* 1024);
  //   actual0 = std::string{};
  //   while (t0.valid()) {
  //     auto read = r0.Read(std::move(t0)).get();
  //     ASSERT_STATUS_OK(read);
  //     ReadPayload p;
  //     AsyncToken t;
  //     std::tie(p, t) = *std::move(read);
  //     t0 = std::move(t);
  //   }
  // }

  //   auto ans = block + block + block;
  // EXPECT_EQ(1,2);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC