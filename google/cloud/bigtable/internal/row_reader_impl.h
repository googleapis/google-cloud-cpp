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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROW_READER_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROW_READER_IMPL_H

#include "google/cloud/bigtable/row.h"
#include "absl/types/variant.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Implementation interface for `bigtable::RowReader`
class RowReaderImpl {
 public:
  virtual ~RowReaderImpl() = default;

  virtual void Cancel() = 0;

  virtual absl::variant<Status, bigtable::Row> Advance() = 0;
};

/**
 * A simple `RowReader` implementation that returns a `Status`.
 *
 * An OK `StatusCode` represents a stream with no rows. Any other `StatusCode`
 * represents a stream with an immediate failure.
 */
class StatusOnlyRowReader : public RowReaderImpl {
 public:
  explicit StatusOnlyRowReader(Status s) : status_(std::move(s)) {}

  void Cancel() override{};

  absl::variant<Status, bigtable::Row> Advance() override { return status_; }

 private:
  Status status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROW_READER_IMPL_H
