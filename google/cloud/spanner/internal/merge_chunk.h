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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_MERGE_CHUNK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_MERGE_CHUNK_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include <google/protobuf/struct.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * Merges @p chunk into @p value, or returns an error.
 *
 * The official documentation about how to "unchunk" Spanner values is at:
 * https://github.com/googleapis/googleapis/blob/master/google/spanner/v1/result_set.proto
 *
 * A paraphrased summary is as follows:
 *
 * * bool/number/null are never chunked and therefore cannot be merged
 * * strings should be concatenated
 * * lists should be concatenated
 *
 * The above rules should be applied recursively.
 *
 * @note The above linked documentation explains how to "unchunk" objects,
 *     which are `google::protobuf::Value` objects with the `struct_value`
 *     field set. However, Spanner never returns these struct_values, so it is
 *     therefore an error to try to merge them.
 */
Status MergeChunk(google::protobuf::Value& value,
                  google::protobuf::Value&& chunk);

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_MERGE_CHUNK_H
