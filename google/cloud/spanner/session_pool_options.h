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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_SESSION_POOL_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_SESSION_POOL_OPTIONS_H_

#include <chrono>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

// What action to take if the session pool is exhausted.
enum class ActionOnExhaustion { BLOCK, FAIL };

struct SessionPoolOptions {
  // The minimum number of sessions to keep in the pool.
  // Values <= 0 are treated as 0.
  // This value will be reduced if it exceeds the overall limit on the number
  // of sessions (`max_sessions_per_channel` * number of channels).
  int min_sessions = 0;

  // The maximum number of sessions to create on each channel.
  // Values <= 1 are treated as 1.
  int max_sessions_per_channel = 100;

  // The maximum number of sessions that can be in the pool in an idle state.
  // Values <= 0 are treated as 0.
  int max_idle_sessions = 0;

  // Decide whether to block or fail on pool exhaustion.
  ActionOnExhaustion action_on_exhaustion = ActionOnExhaustion::BLOCK;

  // This is the interval at which we refresh sessions so they don't get
  // collected by the backend GC. The GC collects objects older than 60
  // minutes, so any duration below that (less some slack to allow the calls
  // to be made to refresh the sessions) should suffice.
  std::chrono::minutes keep_alive_interval = std::chrono::minutes(55);

  // The labels used when creating sessions within the pool.
  //  * Label keys must match `[a-z]([-a-z0-9]{0,61}[a-z0-9])?`.
  //  * Label values must match `([a-z]([-a-z0-9]{0,61}[a-z0-9])?)?`.
  //  * The maximum number of labels is 64.
  std::map<std::string, std::string> labels;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_SESSION_POOL_OPTIONS_H_
