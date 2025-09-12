// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/object_descriptor.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

absl::optional<google::storage::v2::Object> ObjectDescriptor::metadata() const {
  return impl_->metadata();
}

std::pair<AsyncReader, AsyncToken> ObjectDescriptor::Read(std::int64_t offset,
                                                          std::int64_t limit) {
  // TODO: This change is causing performance regression. We need to revisit it
  // after benchmarking our code.

  // std::int64_t max_range =
  // impl_->options().get<storage_experimental::MaximumRangeSizeOption>();
  // if (limit > max_range) {
  //   impl_->MakeSubsequentStream();
  // }
  auto reader = impl_->Read({offset, limit});
  auto token = storage_internal::MakeAsyncToken(reader.get());
  return {AsyncReader(std::move(reader)), std::move(token)};
}

std::pair<AsyncReader, AsyncToken> ObjectDescriptor::ReadFromOffset(
    std::int64_t offset) {
  auto reader = impl_->Read({offset, 0});
  auto token = storage_internal::MakeAsyncToken(reader.get());
  return {AsyncReader(std::move(reader)), std::move(token)};
}

std::pair<AsyncReader, AsyncToken> ObjectDescriptor::ReadLast(
    std::int64_t limit) {
  auto reader = impl_->Read({-limit, 0});
  auto token = storage_internal::MakeAsyncToken(reader.get());
  return {AsyncReader(std::move(reader)), std::move(token)};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
