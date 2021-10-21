// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

class MockCompletionQueueImpl : public internal::CompletionQueueImpl {
 public:
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void, CancelAll, (), (override));
  MOCK_METHOD(future<StatusOr<std::chrono::system_clock::time_point>>,
              MakeDeadlineTimer, (std::chrono::system_clock::time_point),
              (override));
  MOCK_METHOD(future<StatusOr<std::chrono::system_clock::time_point>>,
              MakeRelativeTimer, (std::chrono::nanoseconds), (override));
  MOCK_METHOD(void, RunAsync, (std::unique_ptr<internal::RunAsyncBase>),
              (override));

  MOCK_METHOD(void, StartOperation,
              (std::shared_ptr<internal::AsyncGrpcOperation>,
               absl::FunctionRef<void(void*)>),
              (override));

  MOCK_METHOD(grpc::CompletionQueue&, cq, (), (override));
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_COMPLETION_QUEUE_IMPL_H
