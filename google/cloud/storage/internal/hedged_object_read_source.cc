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
#include "google/cloud/internal/make_status.h"
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/provider.h>
#include <atomic>
#include <cstring>
#include <future>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

HedgedReadMetrics::HedgedReadMetrics()
    : HedgedReadMetrics(opentelemetry::metrics::Provider::GetMeterProvider()) {}

HedgedReadMetrics::HedgedReadMetrics(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider) {
  if (!provider) return;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter =
      provider->GetMeter("gl-cpp", version_string());
  if (!meter) return;

  hedges_dispatched_ = meter->CreateUInt64Counter(
      "storage.read_hedging.hedges_dispatched",
      "Total number of speculative hedge read attempts dispatched", "{hedge}");
  hedge_won_ = meter->CreateUInt64Counter(
      "storage.read_hedging.hedge_won",
      "Total number of hedged read operations won by a secondary hedge attempt",
      "{request}");
}

void HedgedReadMetrics::IncrementHedgesDispatched() {
  if (hedges_dispatched_) hedges_dispatched_->Add(1);
}

void HedgedReadMetrics::IncrementHedgeWon() {
  if (hedge_won_) hedge_won_->Add(1);
}

namespace {

struct RaceResult {
  StatusOr<ReadSourceResult> result;
  std::unique_ptr<ObjectReadSource> source;
  std::unique_ptr<char[]> buffer;
  bool is_primary;
};

struct RaceState {
  std::promise<RaceResult> promise;
  std::atomic<bool> resolved{false};
};

// Opens a new child and performs its initial read, resolving the race if this
// attempt finishes first. Losing attempts close their child. Only the primary
// attempt resolves the race on open or read errors: a hedge that fails must
// not mask a slower, but successful, primary.
void RunAttempt(std::shared_ptr<RaceState> const& state,
                HedgedObjectReadSource::ChildFactory const& factory,
                std::size_t n, bool resolve_on_open_error,
                std::shared_ptr<HedgingThreadPool> release_slot,
                bool is_primary) {
  // Releases the acquired hedge concurrency slot upon function exit across
  // all code paths (early return on open/allocation error, race winner, or
  // race loser). For primary attempts, release_slot is nullptr.
  struct SlotGuard {
    std::shared_ptr<HedgingThreadPool> pool;
    ~SlotGuard() {
      if (pool) pool->ReleaseHedgeSlot();
    }
  } guard{std::move(release_slot)};

  StatusOr<std::unique_ptr<ObjectReadSource>> source = factory();
  if (!source) {
    if (!resolve_on_open_error) return;
    bool expected = false;
    if (state->resolved.compare_exchange_strong(expected, true)) {
      state->promise.set_value(
          RaceResult{std::move(source).status(), nullptr, {}, is_primary});
    }
    return;
  }
  std::unique_ptr<char[]> buffer(new (std::nothrow) char[n]);
  if (!buffer) {
    if (!resolve_on_open_error) return;
    bool expected = false;
    if (state->resolved.compare_exchange_strong(expected, true)) {
      state->promise.set_value(RaceResult{
          google::cloud::internal::ResourceExhaustedError(
              "Out of memory allocating hedge buffer", GCP_ERROR_INFO()),
          nullptr,
          {},
          is_primary});
    }
    return;
  }
  StatusOr<ReadSourceResult> result = (*source)->Read(buffer.get(), n);
  if (!result && !resolve_on_open_error) {
    (*source)->Close();
    return;
  }
  bool expected = false;
  if (state->resolved.compare_exchange_strong(expected, true)) {
    state->promise.set_value(RaceResult{std::move(result), *std::move(source),
                                        std::move(buffer), is_primary});
  } else {
    (*source)->Close();
  }
}

void DispatchHedges(std::shared_ptr<RaceState> const& state,
                    std::future<RaceResult>& future, std::size_t n,
                    int max_hedges, std::chrono::milliseconds delay,
                    std::shared_ptr<HedgingThreadPool> const& hedge_pool,
                    HedgedObjectReadSource::ChildFactory const& child_factory,
                    HedgedReadMetrics* metrics) {
  for (int hedges_dispatched = 0; hedges_dispatched < max_hedges;) {
    if (future.wait_for(delay) != std::future_status::timeout) break;
    if (!hedge_pool->TryAcquireHedgeToken()) {
      // When delay is 0ms (or token acquisition fails), back off briefly on
      // the future instead of busy-spinning if tokens or concurrency slots are
      // temporarily exhausted.
      if (delay == std::chrono::milliseconds::zero() &&
          future.wait_for(std::chrono::milliseconds(10)) !=
              std::future_status::timeout) {
        break;
      }
      continue;
    }
    auto hedge = [state, factory = child_factory, n, pool = hedge_pool] {
      RunAttempt(state, factory, n, /*resolve_on_open_error=*/false, pool,
                 /*is_primary=*/false);
    };
    if (!hedge_pool->Enqueue(hedge)) {
      hedge_pool->ReleaseHedgeSlot();
      break;
    }
    ++hedges_dispatched;
    if (metrics) metrics->IncrementHedgesDispatched();
  }
}

}  // namespace

HedgedObjectReadSource::HedgedObjectReadSource(
    std::shared_ptr<ThreadPool> read_pool,
    std::shared_ptr<HedgingThreadPool> hedge_pool, ChildFactory child_factory,
    std::chrono::milliseconds delay, int max_hedges, std::size_t max_buffer)
    : HedgedObjectReadSource(std::move(read_pool), std::move(hedge_pool),
                             std::move(child_factory), delay, max_hedges,
                             max_buffer, nullptr) {}

HedgedObjectReadSource::HedgedObjectReadSource(
    std::shared_ptr<ThreadPool> read_pool,
    std::shared_ptr<HedgingThreadPool> hedge_pool, ChildFactory child_factory,
    std::chrono::milliseconds delay, int max_hedges, std::size_t max_buffer,
    std::shared_ptr<HedgedReadMetrics> metrics)
    : read_pool_(std::move(read_pool)),
      hedge_pool_(std::move(hedge_pool)),
      child_factory_(std::move(child_factory)),
      delay_(delay),
      max_hedges_(max_hedges),
      max_buffer_(max_buffer),
      metrics_(std::move(metrics)) {}

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

  // Only the stream open is hedged. Once a child has won the race all
  // subsequent reads continue on it, at its current offset, without any
  // thread hops or extra copies.
  if (active_child_) return active_child_->Read(buf, n);

  // Racing requires one staging buffer of `n` bytes per attempt, on top of the
  // caller's own buffer. For a large read that multiplication is worse than
  // the tail latency it avoids, so open the stream without hedging and read
  // straight into the caller's buffer.
  if (n > max_buffer_) {
    StatusOr<std::unique_ptr<ObjectReadSource>> child = child_factory_();
    if (!child) return std::move(child).status();
    active_child_ = *std::move(child);
    return active_child_->Read(buf, n);
  }

  auto state = std::make_shared<RaceState>();
  std::future<RaceResult> future = state->promise.get_future();

  auto primary = [state, factory = child_factory_, n] {
    RunAttempt(state, factory, n, /*resolve_on_open_error=*/true, nullptr,
               /*is_primary=*/true);
  };
  // The primary attempt is scheduled on the dedicated read pool.
  // If the pool is shutting down run the attempt inline, the read must
  // complete either way.
  if (!read_pool_->Enqueue(primary)) primary();

  DispatchHedges(state, future, n, max_hedges_, delay_, hedge_pool_,
                 child_factory_, metrics_.get());

  RaceResult race = future.get();
  if (metrics_ && !race.is_primary && race.result.ok()) {
    metrics_->IncrementHedgeWon();
  }

  active_child_ = std::move(race.source);
  if (race.result.ok() && race.result->bytes_received > 0) {
    std::memcpy(buf, race.buffer.get(), race.result->bytes_received);
  }
  return race.result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
