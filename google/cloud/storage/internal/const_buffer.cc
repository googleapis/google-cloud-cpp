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

#include "google/cloud/storage/internal/const_buffer.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

void PopFrontBytes(ConstBufferSequence& s, std::size_t count) {
  auto i = s.begin();
  for (; i != s.end() && i->size() <= count; ++i) {
    count -= i->size();
  }
  if (i == s.end()) {
    s.clear();
    return;
  }
  // In practice this is expected to be cheap, most vectors will contain 1
  // or 2 elements. And, if you are really lucky, your compiler turns this
  // into a memmove():
  //     https://godbolt.org/z/jw5VDd
  s.erase(s.begin(), i);
  if (count > 0 && !s.empty()) {
    s.front() = ConstBuffer(s.front().data() + count, s.front().size() - count);
  }
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
