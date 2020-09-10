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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

// Tests typically create an instance of this class, then create a
// `google::cloud::bigtable::CompletionQueue` to wrap it, keeping a reference to
// the instance to manipulate its state directly.
class FakeCompletionQueueImpl
    : public google::cloud::internal::CompletionQueueImpl {
 public:
  std::unique_ptr<grpc::Alarm> CreateAlarm() const override;

  using CompletionQueueImpl::empty;
  using CompletionQueueImpl::SimulateCompletion;
  using CompletionQueueImpl::size;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_COMPLETION_QUEUE_IMPL_H
