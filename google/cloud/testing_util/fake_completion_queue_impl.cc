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

#include "google/cloud/testing_util/fake_completion_queue_impl.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

std::unique_ptr<grpc::Alarm> FakeCompletionQueueImpl::CreateAlarm() const {
  // grpc::Alarm objects are really hard to cleanup when mocking their
  // behavior, so we do not create an alarm, instead we return nullptr, which
  // the classes that care (AsyncTimerFunctor) know what to do with.
  return std::unique_ptr<grpc::Alarm>();
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
