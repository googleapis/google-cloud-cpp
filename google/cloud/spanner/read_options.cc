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

#include "google/cloud/spanner/read_options.h"
#include "google/cloud/spanner/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options ToOptions(ReadOptions const& ro) {
  Options opts;
  if (!ro.index_name.empty()) {
    opts.set<ReadIndexNameOption>(ro.index_name);
  }
  if (ro.limit != 0) {
    opts.set<ReadRowLimitOption>(ro.limit);
  }
  if (ro.request_priority) {
    opts.set<RequestPriorityOption>(*ro.request_priority);
  }
  if (ro.request_tag) {
    opts.set<RequestTagOption>(*ro.request_tag);
  }
  return opts;
}

ReadOptions ToReadOptions(Options const& opts) {
  ReadOptions ro;
  if (opts.has<ReadIndexNameOption>()) {
    ro.index_name = opts.get<ReadIndexNameOption>();
  }
  if (opts.has<ReadRowLimitOption>()) {
    ro.limit = opts.get<ReadRowLimitOption>();
  }
  if (opts.has<RequestPriorityOption>()) {
    ro.request_priority = opts.get<RequestPriorityOption>();
  }
  if (opts.has<RequestTagOption>()) {
    ro.request_tag = opts.get<RequestTagOption>();
  }
  return ro;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
