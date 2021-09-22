// Copyright 2021 Google LLC
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

#include "google/cloud/background_threads_factory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(BackgroundThreadsFactory, CustomBackgroundThreads) {
  using ms = std::chrono::milliseconds;
  CompletionQueue cq;
  auto background = CustomBackgroundThreads(cq)();

  // Schedule some work that cannot execute because there is no thread draining
  // the completion queue.
  promise<std::thread::id> p;
  auto background_thread_id = p.get_future();
  background->cq().RunAsync(
      [&p](CompletionQueue&) { p.set_value(std::this_thread::get_id()); });
  EXPECT_NE(std::future_status::ready, background_thread_id.wait_for(ms(10)));

  // Verify we can create our own threads to drain the completion queue.
  std::thread t([&cq] { cq.Run(); });
  EXPECT_EQ(t.get_id(), background_thread_id.get());

  cq.Shutdown();
  t.join();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
