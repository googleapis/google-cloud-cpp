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
#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Hedge reads of an `ObjectReadSource` to reduce tail latency.
 *
 * The first `Read()` (the stream open) races a primary attempt against up to
 * @p max_hedges additional attempts created by @p child_factory, each started
 * after @p delay elapses without a winner. The first attempt to complete its
 * read wins and becomes the active child; losing attempts are closed when they
 * eventually complete.
 *
 * Later reads normally continue on the active child, on the caller's thread,
 * with no thread hops or copies. A read that takes longer than @p delay marks
 * the stream as stalled, and the next read is raced again: the active child is
 * the primary attempt, and hedges are opened by @p child_factory at the
 * stream's current offset, pinned to the generation observed so far. A hedge
 * that wins replaces the active child. Once a read completes within @p delay
 * the stream goes back to direct reads.
 *
 * Racing is skipped where it cannot produce correct data or cannot help: under
 * decompressive transcoding (byte ranges are not honored, a hedge would restart
 * from the first byte), for reads larger than @p max_buffer (each attempt
 * stages its own copy of the data, so a large read would multiply memory use),
 * and once the stream has reached the end of the requested data (a hedge would
 * request an empty or invalid range).
 */
class HedgedObjectReadSource : public ObjectReadSource {
 public:
  /**
   * Creates a child stream positioned at @p current_offset.
   *
   * With `kFromBeginning` the offset counts bytes from the start of the object,
   * with `kFromEnd` it is the number of bytes still to read from the end of the
   * object (`ReadLast`). The child must read the given @p generation when one
   * is known.
   */
  using ChildFactory =
      std::function<StatusOr<std::unique_ptr<ObjectReadSource>>(
          std::int64_t current_offset, std::optional<std::int64_t> generation)>;

  /// Where the stream starts, as derived from the original request.
  struct Position {
    /// Bytes from the start of the object, or for `ReadLast` the bytes
    /// remaining to read from the end of the object.
    std::int64_t offset = 0;
    OffsetDirection direction = kFromBeginning;
    /// Exclusive end of the requested range, if the request has one.
    std::optional<std::int64_t> end_offset;
    std::optional<std::int64_t> generation;
  };

  HedgedObjectReadSource(std::shared_ptr<ThreadPool> read_pool,
                         std::shared_ptr<HedgingThreadPool> hedge_pool,
                         ChildFactory child_factory,
                         std::chrono::milliseconds delay, int max_hedges,
                         std::size_t max_buffer, Position position);

  /// A stream that starts at the beginning of the object.
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
  bool ShouldRace(std::size_t n) const;
  bool AtEnd() const;
  StatusOr<ReadSourceResult> ReadDirect(char* buf, std::size_t n);
  StatusOr<ReadSourceResult> ReadRaced(char* buf, std::size_t n);
  void UpdateState(StatusOr<ReadSourceResult> const& result);

  std::shared_ptr<ThreadPool> read_pool_;
  std::shared_ptr<HedgingThreadPool> hedge_pool_;
  // Shared with the racing attempts, which may outlive this object.
  std::shared_ptr<ChildFactory const> child_factory_;
  std::chrono::milliseconds delay_;
  int max_hedges_;
  std::size_t max_buffer_;

  std::int64_t current_offset_;
  OffsetDirection offset_direction_;
  std::optional<std::int64_t> end_offset_;
  std::optional<std::int64_t> generation_;
  std::optional<std::uint64_t> size_;
  bool is_gunzipped_ = false;
  bool last_read_stalled_ = false;

  // The staging buffer of the last winning attempt, reused by the next race.
  std::unique_ptr<char[]> staging_buffer_;
  std::size_t staging_buffer_capacity_ = 0;

  std::unique_ptr<ObjectReadSource> active_child_;
  bool is_closed_ = false;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_OBJECT_READ_SOURCE_H
