// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc/make_cord.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

absl::Cord MakeCord(std::string p) {
  // The absl::Cord constructor from `std::string` splits the string into many
  // small buffers (and allocates a container for each). We want to avoid copies
  // and allocations.
  auto holder = std::make_shared<std::string>(std::move(p));
  auto contents = absl::string_view(holder->data(), holder->size());
  return absl::MakeCordFromExternal(contents,
                                    [b = std::move(holder)]() mutable {});
}

absl::Cord MakeCord(std::vector<std::string> p) {
  return std::accumulate(std::make_move_iterator(p.begin()),
                         std::make_move_iterator(p.end()), absl::Cord(),
                         [](absl::Cord a, std::string b) {
                           a.Append(MakeCord(std::move(b)));
                           return a;
                         });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
