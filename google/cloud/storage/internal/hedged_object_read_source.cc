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

#include "google/cloud/storage/internal/hedged_object_read_source.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/internal/make_status.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <mutex>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

// The number of races a single stream may hedge, as a multiple of
// `max_hedges`. `max_hedges` bounds one race; without this budget a stream
// that is merely slow (every read takes longer than the hedge delay) would
// re-race on every read and issue an unbounded number of duplicate requests,
// because the rate limit and concurrency backstops are both disabled by
// default.
int constexpr kMaxHedgeRoundsPerStream = 8;

struct RaceResult {
  StatusOr<ReadSourceResult> result;
  std::unique_ptr<ObjectReadSource> source;
  std::unique_ptr<char[]> buffer;
  std::size_t buffer_capacity = 0;
};

// Shared between the caller, which schedules the attempts and waits for the
// winner, and the attempts themselves, which may outlive the caller's wait.
struct RaceState {
  std::promise<RaceResult> promise;
  std::atomic<bool> resolved{false};
  std::atomic<int> active_attempts{0};

  // The primary attempt reads from the active child (if any) into the staging
  // buffer kept from the previous race (if large enough). Both are consumed
  // by the primary attempt when it starts.
  std::unique_ptr<ObjectReadSource> primary_child;
  std::unique_ptr<char[]> primary_buffer;
  std::size_t primary_buffer_capacity = 0;

  std::mutex mu;
  Status primary_error;  // GUARDED_BY(mu)
  Status last_error;     // GUARDED_BY(mu)

  // Returns true for exactly one caller: the one that gets to set the result.
  bool TryClaim() {
    bool expected = false;
    return resolved.compare_exchange_strong(expected, true);
  }

  // The error reported when every attempt fails. The primary describes the
  // stream the caller is actually reading, so its error takes precedence over
  // whatever a hedge happened to fail with last.
  Status FinalError() {
    std::lock_guard<std::mutex> lock(mu);
    if (!primary_error.ok()) return primary_error;
    return last_error;
  }

  // Called once for every attempt that ends without a result: an open error,
  // a read error, or a hedge that could not be dispatched. The last attempt
  // to retire resolves the race with the collected error.
  void RetireAttempt() {
    if (active_attempts.fetch_sub(1) != 1) return;
    if (!TryClaim()) return;
    promise.set_value(RaceResult{FinalError(), nullptr, nullptr});
  }

  void Fail(Status status, bool is_primary) {
    bool const permanent =
        is_primary && StatusTraits::IsPermanentFailure(status);
    {
      std::lock_guard<std::mutex> lock(mu);
      if (is_primary) {
        primary_error = std::move(status);
      } else {
        last_error = std::move(status);
      }
    }
    // A hedge cannot fix a permanent error on the primary (the object is gone,
    // access is denied, ...). Report it now instead of holding the caller
    // until every in-flight hedge has exhausted its own retry budget.
    if (permanent && TryClaim()) {
      promise.set_value(RaceResult{FinalError(), nullptr, nullptr});
    }
    RetireAttempt();
  }
};

// Runs a single read attempt. The primary attempt reads from the active child
// when the stream has one, any other attempt opens a new child at @p offset
// and @p generation. A successful read resolves the race immediately; the
// loser closes its own child. A failed attempt only resolves the race if it
// is the last one standing, or if it is the primary failing permanently.
void RunAttempt(std::shared_ptr<RaceState> const& state,
                HedgedObjectReadSource::ChildFactory const& factory,
                std::unique_ptr<ObjectReadSource> child,
                std::unique_ptr<char[]> buffer, std::size_t buffer_capacity,
                std::int64_t offset, std::optional<std::int64_t> generation,
                std::size_t n, bool is_primary,
                std::weak_ptr<HedgingThreadPool> release_slot) {
  // Releases the acquired hedge concurrency slot upon function exit across
  // all code paths. For the primary attempt, release_slot is empty.
  //
  // std::weak_ptr is used intentionally instead of std::shared_ptr: tasks
  // executing inside HedgingThreadPool::pool_ must not hold a strong reference
  // to HedgingThreadPool, otherwise an in-flight losing hedge task would
  // create a reference cycle and prevent ~HedgingThreadPool() from running on
  // the owning thread when the last external shared_ptr is dropped (causing
  // the worker thread to outlive the caller and detach instead of being
  // joined).
  struct SlotGuard {
    std::weak_ptr<HedgingThreadPool> pool;
    ~SlotGuard() {
      if (std::shared_ptr<HedgingThreadPool> p = pool.lock()) {
        p->ReleaseHedgeSlot();
      }
    }
  } guard{std::move(release_slot)};

  if (!child) {
    StatusOr<std::unique_ptr<ObjectReadSource>> source =
        factory(offset, generation);
    if (!source) return state->Fail(std::move(source).status(), is_primary);
    child = *std::move(source);
  }

  if (!buffer) {
    buffer.reset(new (std::nothrow) char[n]);
    if (!buffer) {
      return state->Fail(
          google::cloud::internal::ResourceExhaustedError(
              "Out of memory allocating hedge buffer", GCP_ERROR_INFO()),
          is_primary);
    }
    buffer_capacity = n;
  }

  StatusOr<ReadSourceResult> result = child->Read(buffer.get(), n);
  if (!result) {
    // A child that failed may have already torn down its connection, e.g. a
    // `RetryObjectReadSource` that exhausted its retry policy has no child of
    // its own to close.
    if (child->IsOpen()) child->Close();
    return state->Fail(std::move(result).status(), is_primary);
  }

  if (!state->TryClaim()) {
    // Lost the race, the winner's data was already returned to the caller.
    child->Close();
    return;
  }
  state->promise.set_value(RaceResult{std::move(result), std::move(child),
                                      std::move(buffer), buffer_capacity});
}

}  // namespace

HedgedObjectReadSource::HedgedObjectReadSource(
    std::shared_ptr<ThreadPool> read_pool,
    std::shared_ptr<HedgingThreadPool> hedge_pool, ChildFactory child_factory,
    std::chrono::milliseconds delay, int max_hedges, std::size_t max_buffer,
    Position position)
    : read_pool_(std::move(read_pool)),
      hedge_pool_(std::move(hedge_pool)),
      child_factory_(
          std::make_shared<ChildFactory const>(std::move(child_factory))),
      delay_(delay),
      max_hedges_(max_hedges),
      max_buffer_(max_buffer),
      current_offset_(position.offset),
      offset_direction_(position.direction),
      end_offset_(position.end_offset),
      generation_(position.generation) {}

HedgedObjectReadSource::HedgedObjectReadSource(
    std::shared_ptr<ThreadPool> read_pool,
    std::shared_ptr<HedgingThreadPool> hedge_pool, ChildFactory child_factory,
    std::chrono::milliseconds delay, int max_hedges, std::size_t max_buffer)
    : HedgedObjectReadSource(std::move(read_pool), std::move(hedge_pool),
                             std::move(child_factory), delay, max_hedges,
                             max_buffer, Position{}) {}

bool HedgedObjectReadSource::IsOpen() const {
  if (active_child_) return active_child_->IsOpen();
  return !is_closed_;
}

StatusOr<HttpResponse> HedgedObjectReadSource::Close() {
  is_closed_ = true;
  if (active_child_) return active_child_->Close();
  // The source was never read from, there is no child (or HTTP response) to
  // close.
  return HttpResponse{HttpStatusCode::kOk, {}, {}};
}

StatusOr<ReadSourceResult> HedgedObjectReadSource::Read(char* buf,
                                                        std::size_t n) {
  if (is_closed_) {
    return ReadSourceResult{0, HttpResponse{HttpStatusCode::kOk, {}, {}}};
  }
  StatusOr<ReadSourceResult> result =
      ShouldRace(n) ? ReadRaced(buf, n) : ReadDirect(buf, n);
  UpdateState(result);
  return result;
}

bool HedgedObjectReadSource::ShouldRace(std::size_t n) const {
  if (max_hedges_ <= 0 || !read_pool_ || !hedge_pool_) return false;
  // Racing stages one copy of `n` bytes per attempt on top of the caller's
  // buffer. For a large read that multiplication is worse than the tail
  // latency it avoids.
  if (n > max_buffer_) return false;
  // The stream open is always raced, that is where most tail latency lives.
  if (!active_child_) return true;
  // Decompressive transcoding does not respect byte ranges (HTTP 206). A
  // mid-stream hedge would have to re-read and discard from offset 0, which
  // is worse than reading directly on the active child.
  if (is_gunzipped_) return false;
  // The caller drains a stream with one more read at the end of the requested
  // data. A hedge there would request an empty or inverted range, and could
  // even win the race with bytes from the wrong offset.
  if (AtEnd()) return false;
  // Every read is raced, so a uniformly slow stream would otherwise dispatch a
  // hedge on every read for the life of the stream.
  if (total_hedges_ >= max_hedges_ * kMaxHedgeRoundsPerStream) return false;
  // Every remaining read is raced. Racing does not dispatch a hedge on its
  // own: `ReadRaced()` only does that once `delay_` elapses within this read,
  // so a read that returns promptly still issues exactly one request.
  //
  // The race is what makes that elapsed time observable. `ReadDirect()` calls
  // `active_child_->Read()` synchronously on the caller's thread, so while a
  // read is stalled there is no thread left to notice. Gating on whether a
  // *previous* read stalled cannot rescue the first stall on a stream that
  // opened cleanly, which is the common case for a short ranged read.
  return true;
}

bool HedgedObjectReadSource::AtEnd() const {
  if (offset_direction_ == kFromEnd) return current_offset_ <= 0;
  // An explicit range end is authoritative. `size_` comes from the response,
  // and on the REST path it falls back to `content-length`, which is the
  // length of that response rather than the size of the object. For a ranged
  // read that is smaller than the stream's offset, so consulting it here would
  // end the stream on the first check.
  if (end_offset_) return current_offset_ >= *end_offset_;
  return size_ && current_offset_ >= static_cast<std::int64_t>(*size_);
}

StatusOr<ReadSourceResult> HedgedObjectReadSource::ReadDirect(char* buf,
                                                              std::size_t n) {
  if (!active_child_) {
    StatusOr<std::unique_ptr<ObjectReadSource>> child =
        (*child_factory_)(current_offset_, generation_);
    if (!child) {
      // The stream never opened, there is nothing to read from or to close.
      is_closed_ = true;
      return std::move(child).status();
    }
    active_child_ = *std::move(child);
  }
  StatusOr<ReadSourceResult> result = active_child_->Read(buf, n);
  if (!result) {
    // Match `ReadRaced()`: a child whose read failed has exhausted its retry
    // policy, there is nothing left to read from. A child that is still open
    // holds a connection that must be released.
    if (active_child_->IsOpen()) active_child_->Close();
    active_child_.reset();
    is_closed_ = true;
  }
  return result;
}

StatusOr<ReadSourceResult> HedgedObjectReadSource::ReadRaced(char* buf,
                                                             std::size_t n) {
  auto state = std::make_shared<RaceState>();
  std::future<RaceResult> future = state->promise.get_future();
  state->active_attempts.store(1);
  state->primary_child = std::move(active_child_);
  if (staging_buffer_capacity_ >= n) {
    state->primary_buffer = std::move(staging_buffer_);
    state->primary_buffer_capacity = staging_buffer_capacity_;
  }
  staging_buffer_.reset();
  staging_buffer_capacity_ = 0;

  auto primary = [state, factory = child_factory_, offset = current_offset_,
                  gen = generation_, n] {
    RunAttempt(state, *factory, std::move(state->primary_child),
               std::move(state->primary_buffer), state->primary_buffer_capacity,
               offset, gen, n,
               /*is_primary=*/true, std::weak_ptr<HedgingThreadPool>{});
  };
  // The primary attempt is scheduled on the dedicated read pool.
  // If the pool is shutting down run the attempt inline, the read must
  // complete either way.
  if (!read_pool_->Enqueue(primary)) primary();

  for (int hedges_dispatched = 0; hedges_dispatched < max_hedges_;) {
    if (future.wait_for(delay_) != std::future_status::timeout) break;
    if (!hedge_pool_->TryAcquireHedgeToken()) {
      // When delay_ is 0ms (or token acquisition fails), back off briefly on
      // the future instead of busy-spinning if tokens or concurrency slots are
      // temporarily exhausted.
      if (delay_ == std::chrono::milliseconds::zero()) {
        if (future.wait_for(std::chrono::milliseconds(10)) !=
            std::future_status::timeout) {
          break;
        }
      }
      continue;
    }
    state->active_attempts.fetch_add(1);
    auto hedge = [state, factory = child_factory_, offset = current_offset_,
                  gen = generation_, n,
                  pool = std::weak_ptr<HedgingThreadPool>(hedge_pool_)] {
      RunAttempt(state, *factory, /*child=*/nullptr, /*buffer=*/nullptr,
                 /*buffer_capacity=*/0, offset, gen, n, /*is_primary=*/false,
                 pool);
    };
    if (!hedge_pool_->Enqueue(hedge)) {
      hedge_pool_->ReleaseHedgeSlot();
      state->RetireAttempt();
      break;
    }
    ++hedges_dispatched;
    ++total_hedges_;
  }

  RaceResult race = future.get();
  active_child_ = std::move(race.source);
  if (!race.result) {
    // Every attempt failed and closed its own child, there is nothing left to
    // read from or to close.
    is_closed_ = true;
    return std::move(race.result).status();
  }
  if (race.result->bytes_received > 0) {
    std::memcpy(buf, race.buffer.get(), race.result->bytes_received);
  }
  staging_buffer_ = std::move(race.buffer);
  staging_buffer_capacity_ = race.buffer_capacity;
  return race.result;
}

void HedgedObjectReadSource::UpdateState(
    StatusOr<ReadSourceResult> const& result) {
  if (!result) return;
  if (result->generation) generation_ = result->generation;
  if (result->size && !size_) size_ = result->size;
  if (result->transformation.value_or("") == "gunzipped") {
    // Decompressive transcoding does not respect byte ranges, so `ShouldRace()`
    // disengages for the rest of the stream once this is set. No hedge will
    // reopen the object, so there is no resume position left to track.
    is_gunzipped_ = true;
    return;
  }
  auto const received = static_cast<std::int64_t>(result->bytes_received);
  if (offset_direction_ == kFromEnd) {
    // `ReadLast(N)` with `N` larger than the object returns the whole object.
    // The bytes still to read are then bounded by the object size, not by
    // `N`, otherwise a hedge opened with the remaining count would cover the
    // whole object again and return data from the first byte.
    if (size_ && current_offset_ > static_cast<std::int64_t>(*size_)) {
      current_offset_ = static_cast<std::int64_t>(*size_);
    }
    current_offset_ -= received;
  } else {
    current_offset_ += received;
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
