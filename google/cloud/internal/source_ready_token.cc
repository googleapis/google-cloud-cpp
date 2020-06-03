// Copyright 2020 Google LLC
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

#include "google/cloud/internal/source_ready_token.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

future<ReadyToken> ReadyTokenFlowControl::Acquire() {
  std::lock_guard<std::mutex> lk(mu_);
  if (current_outstanding_ < max_outstanding_) {
    ++current_outstanding_;
    return make_ready_future(ReadyToken(this));
  }
  promise<ReadyToken> p;
  auto f = p.get_future();
  pending_.push_back(std::move(p));
  return f;
}

bool ReadyTokenFlowControl::Release(ReadyToken token) {
  std::unique_lock<std::mutex> lk(mu_);
  if (token.value_ != this) return false;
  --current_outstanding_;
  if (current_outstanding_ >= max_outstanding_ || pending_.empty()) return true;

  promise<ReadyToken> p(std::move(pending_.front()));
  pending_.pop_front();
  ++current_outstanding_;
  lk.unlock();
  p.set_value(ReadyToken(this));
  return true;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
