// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_OBJECT_READ_SOURCE_H

#include "google/cloud/storage/internal/hedging_thread_pool.h"
#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Hedge the *open* of an `ObjectReadSource` to reduce tail latency.
 *
 * The first `Read()` races one or more children created by `child_factory`:
 * a primary attempt starts immediately, and up to @p max_hedges additional
 * attempts start, staggered by @p delay, while no attempt has completed. The
 * first attempt to complete its initial read wins; losing attempts are closed
 * when they eventually complete.
 *
 * Only the initial open is hedged. `ObjectReadSource` is a stream, so a hedge
 * started mid-stream would restart from the request's initial offset and
 * could return the wrong bytes. After the race, all subsequent reads simply
 * continue on the winning child at its current offset, with no extra threads
 * or copies.
 *
 * Each racing attempt reads into its own buffer, because a losing attempt
 * keeps writing until it completes and must not touch the caller's buffer.
 * Peak memory for the race is therefore proportional to the size of the first
 * read. Reads larger than @p max_buffer are served without hedging, directly
 * into the caller's buffer, so a large read cannot multiply memory use.
 */
class HedgedObjectReadSource : public ObjectReadSource {
 public:
  using ChildFactory =
      std::function<StatusOr<std::unique_ptr<ObjectReadSource>>()>;

  HedgedObjectReadSource(std::shared_ptr<ThreadPool> read_pool,
                         std::shared_ptr<HedgingThreadPool> hedge_pool,
                         ChildFactory child_factory,
                         std::chrono::milliseconds delay, int max_hedges,
                         std::size_t max_buffer);

  ~HedgedObjectReadSource() override = default;

  bool IsOpen() const override;
  StatusOr<HttpResponse> Close() override;
  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override;

 private:
  std::shared_ptr<ThreadPool> read_pool_;
  std::shared_ptr<HedgingThreadPool> hedge_pool_;
  ChildFactory child_factory_;
  std::chrono::milliseconds delay_;
  int max_hedges_;
  std::size_t max_buffer_;

  std::unique_ptr<ObjectReadSource> active_child_;
  bool is_closed_ = false;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_OBJECT_READ_SOURCE_H
