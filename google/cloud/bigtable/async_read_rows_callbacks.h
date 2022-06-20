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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_READ_ROWS_CALLBACKS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_READ_ROWS_CALLBACKS_H

#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include <functional>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The type of the callback to be invoked on each successfully read row in a
 * `Table::AsyncReadRows(...)` call.
 *
 * The `Row` passed to this callback is the latest row to be read.
 *
 * The returned `future<bool>` should be satisfied with `true` when the user is
 * ready to receive the next callback and with `false` when the user doesn't
 * want any more rows.
 */
using RowFunctor = std::function<future<bool>(Row)>;

/**
 * The type of the callback to be invoked upon the end of a
 * `Table::AsyncReadRows(...)` call.
 *
 * The status passed to this callback is the final status of the operation.
 */
using FinishFunctor = std::function<void(Status)>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_READ_ROWS_CALLBACKS_H
