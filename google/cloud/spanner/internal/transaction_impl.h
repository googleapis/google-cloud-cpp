// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TRANSACTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TRANSACTION_IMPL_H

#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/port_platform.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/transaction.pb.h>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct TransactionContext {
  bool route_to_leader;
  std::string const& tag;
  std::int64_t seqno;
  absl::optional<std::shared_ptr<SpannerStub>> stub;
  absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
      precommit_token;
};

template <typename Functor>
using VisitInvokeResult = ::google::cloud::internal::invoke_result_t<
    Functor, SessionHolder&,
    StatusOr<google::spanner::v1::TransactionSelector>&, TransactionContext&>;

/**
 * The internal representation of a google::cloud::spanner::Transaction.
 */
class TransactionImpl {
 public:
  TransactionImpl(google::spanner::v1::TransactionSelector selector,
                  bool route_to_leader, std::string tag);

  TransactionImpl(TransactionImpl const& impl,
                  google::spanner::v1::TransactionSelector selector,
                  bool route_to_leader, std::string tag);

  TransactionImpl(
      SessionHolder session, google::spanner::v1::TransactionSelector selector,
      bool route_to_leader, std::string tag,
      absl::optional<std::string> multiplexed_session_previous_transaction_id);

  ~TransactionImpl();

  // Visit the transaction with the given functor, which should use (and
  // modify, if appropriate) the passed SessionHolder and TransactionSelector.
  //
  // If the SessionHolder is not nullptr, the functor must use the session.
  // Otherwise it must allocate a session and assign to the SessionHolder.
  //
  // If the TransactionSelector is in the "begin" state and the operation
  // successfully allocates a transaction ID, then the functor must assign
  // that ID to the selector. If the functor fails to allocate a transaction
  // ID then it must assign a `Status` that indicates why transaction
  // allocation failed (i.e. the result of `BeginTransaction`) to the parameter.
  // All of this is independent of whether the functor itself succeeds.
  //
  // If the TransactionSelector is not in the "begin" state then the functor
  // must not modify it. Rather it should use either the transaction ID or
  // the error state in a manner appropriate for the operation.
  //
  // Additional transaction context is also passed to the functor, including
  // a tag string, and a monotonically-increasing sequence number.
  template <typename Functor>
  VisitInvokeResult<Functor> Visit(Functor&& f) {
    static_assert(google::cloud::internal::is_invocable<
                      Functor, SessionHolder&,
                      StatusOr<google::spanner::v1::TransactionSelector>&,
                      TransactionContext&>::value,
                  "TransactionImpl::Visit() functor has incompatible type.");
    TransactionContext ctx{route_to_leader_, tag_, 0, absl::nullopt,
                           absl::nullopt};
    {
      std::unique_lock<std::mutex> lock(mu_);
      ctx.seqno = ++seqno_;  // what about overflow?
      cond_.wait(lock, [this] { return state_ != State::kPending; });
      ctx.stub = stub_;
      ctx.precommit_token = precommit_token_;
      if (state_ == State::kDone) {
        lock.unlock();
        auto result = f(session_, selector_, ctx);
        lock.lock();
        UpdatePrecommitToken(lock, ctx.precommit_token);
        return result;
      }
      state_ = State::kPending;
    }
    // selector_->has_begin(), but only one visitor active at a time.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    try {
#endif
      auto r = f(session_, selector_, ctx);
      bool done = false;
      {
        std::unique_lock<std::mutex> lock(mu_);
        stub_ = ctx.stub;
        UpdatePrecommitToken(lock, ctx.precommit_token);
        state_ =
            selector_ && selector_->has_begin() ? State::kBegin : State::kDone;
        done = (state_ == State::kDone);
      }
      if (done) {
        cond_.notify_all();
      } else {
        cond_.notify_one();
      }
      return r;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(mu_);
        state_ = State::kBegin;
      }
      cond_.notify_one();
      throw;
    }
#endif
  }

 private:
  void UpdatePrecommitToken(
      std::unique_lock<std::mutex> const&,
      absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
          token);

  enum class State {
    kBegin,    // waiting for a future visitor to assign a transaction ID
    kPending,  // waiting for an active visitor to assign a transaction ID
    kDone,     // a transaction ID has been assigned (or we are single-use)
  };
  State state_;

  std::mutex mu_;
  std::condition_variable cond_;
  SessionHolder session_;
  StatusOr<google::spanner::v1::TransactionSelector> selector_;
  bool route_to_leader_;
  std::string tag_;
  std::int64_t seqno_;
  absl::optional<std::shared_ptr<SpannerStub>> stub_ = absl::nullopt;
  absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
      precommit_token_ = absl::nullopt;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TRANSACTION_IMPL_H
