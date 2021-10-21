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

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

template <typename Functor>
using VisitInvokeResult = ::google::cloud::internal::invoke_result_t<
    Functor, SessionHolder&,
    StatusOr<google::spanner::v1::TransactionSelector>&, std::int64_t>;

/**
 * The internal representation of a google::cloud::spanner::Transaction.
 */
class TransactionImpl {
 public:
  explicit TransactionImpl(google::spanner::v1::TransactionSelector selector)
      : TransactionImpl(/*session=*/{}, std::move(selector)) {}

  TransactionImpl(TransactionImpl const& impl,
                  google::spanner::v1::TransactionSelector selector)
      : TransactionImpl(impl.session_, std::move(selector)) {}

  TransactionImpl(SessionHolder session,
                  google::spanner::v1::TransactionSelector selector)
      : session_(std::move(session)),
        selector_(std::move(selector)),
        seqno_(0) {
    state_ = selector_->has_begin() ? State::kBegin : State::kDone;
  }

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
  // A monotonically-increasing sequence number is also passed to the functor.
  template <typename Functor>
  VisitInvokeResult<Functor> Visit(Functor&& f) {
    static_assert(google::cloud::internal::is_invocable<
                      Functor, SessionHolder&,
                      StatusOr<google::spanner::v1::TransactionSelector>&,
                      std::int64_t>::value,
                  "TransactionImpl::Visit() functor has incompatible type.");
    std::int64_t seqno;
    {
      std::unique_lock<std::mutex> lock(mu_);
      seqno = ++seqno_;  // what about overflow?
      cond_.wait(lock, [this] { return state_ != State::kPending; });
      if (state_ == State::kDone) {
        lock.unlock();
        return f(session_, selector_, seqno);
      }
      state_ = State::kPending;
    }
    // selector_->has_begin(), but only one visitor active at a time.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    try {
#endif
      auto r = f(session_, selector_, seqno);
      bool done = false;
      {
        std::lock_guard<std::mutex> lock(mu_);
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
  std::int64_t seqno_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TRANSACTION_IMPL_H
