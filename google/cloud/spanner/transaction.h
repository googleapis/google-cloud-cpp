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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TRANSACTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TRANSACTION_H

#include "google/cloud/spanner/internal/transaction_impl.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/v1/transaction.pb.h>
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class Transaction;  // defined below

// Internal forward declarations to befriend.
namespace internal {
template <typename T>
Transaction MakeSingleUseTransaction(T&&);
template <typename Functor>
VisitInvokeResult<Functor> Visit(Transaction, Functor&&);
Transaction MakeTransactionFromIds(std::string session_id,
                                   std::string transaction_id);
}  // namespace internal

/**
 * The representation of a Cloud Spanner transaction.
 *
 * A transaction is a set of reads and writes that execute atomically at a
 * single logical point in time across the columns/rows/tables in a database.
 * Those reads and writes are grouped by passing them the same `Transaction`.
 *
 * All reads/writes in the transaction must be executed within the same
 * session, and that session may have only one transaction active at a time.
 *
 * Spanner supports these transaction modes:
 *   - ReadOnly. Provides guaranteed consistency across several reads, but does
 *     not allow writes. Can be configured to read at timestamps in the past.
 *     Does not need to be committed and does not take locks.
 *   - ReadWrite. Supports reading and writing data at a single point in time.
 *     Uses pessimistic locking and, if necessary, two-phase commit. May abort,
 *     requiring the application to rerun.
 *   - SingleUse. A restricted form of a ReadOnly transaction where Spanner
 *     chooses the read timestamp.
 */
class Transaction {
 public:
  /**
   * Options for ReadOnly transactions.
   */
  class ReadOnlyOptions {
   public:
    // Strong: Guarantees visibility of the effects of all transactions that
    // committed before the start of the reads.
    ReadOnlyOptions();

    // Exact Staleness: Executes all reads at `read_timestamp`.
    explicit ReadOnlyOptions(Timestamp read_timestamp);

    // Exact Staleness: Executes all reads at a timestamp `exact_staleness`
    // old. The actual timestamp is chosen soon after the reads are started.
    explicit ReadOnlyOptions(std::chrono::nanoseconds exact_staleness);

   private:
    friend Transaction;
    google::spanner::v1::TransactionOptions_ReadOnly ro_opts_;
  };

  /**
   * Options for ReadWrite transactions.
   */
  class ReadWriteOptions {
   public:
    // There are currently no read-write options.
    ReadWriteOptions();

   private:
    friend Transaction;
    google::spanner::v1::TransactionOptions_ReadWrite rw_opts_;
  };

  /**
   * Options for "single-use", ReadOnly transactions, where Spanner chooses
   * the read timestamp, subject to user-provided bounds. This allows reading
   * without blocking.
   *
   * Because selection of the timestamp requires knowledge of which rows will
   * be read, a single-use transaction can only be used with one read. See
   * Client::Read() and Client::ExecuteQuery(). SingleUseOptions cannot be used
   * to construct an application-level Transaction.
   */
  class SingleUseOptions {
   public:
    // Strong or Exact Staleness: See ReadOnlyOptions.
    // NOLINTNEXTLINE(google-explicit-constructor)
    SingleUseOptions(ReadOnlyOptions opts);

    // Bounded Staleness: Executes all reads at a timestamp that is not
    // before `min_read_timestamp`.
    explicit SingleUseOptions(Timestamp min_read_timestamp);

    // Bounded Staleness: Executes all reads at a timestamp that is not
    // before `NOW - max_staleness`.
    explicit SingleUseOptions(std::chrono::nanoseconds max_staleness);

   private:
    friend Transaction;
    google::spanner::v1::TransactionOptions_ReadOnly ro_opts_;
  };

  /// @name Construction of read-only and read-write transactions.
  ///@{
  /**
   * @note This is a lazy evaluated operation. No RPCs are made as part of
   *     creating a `Transaction` object. Instead, the first request to the
   *     server (for example as part of a `ExecuteQuery()` call) will also
   *     create the transaction.
   */
  explicit Transaction(ReadOnlyOptions opts);
  /// @copydoc Transaction(ReadOnlyOptions)
  explicit Transaction(ReadWriteOptions opts);
  /// @copydoc Transaction(ReadOnlyOptions)
  Transaction(Transaction const& txn, ReadWriteOptions opts);
  ///@}

  ~Transaction();

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  Transaction(Transaction&&) = default;
  Transaction& operator=(Transaction&&) = default;
  Transaction(Transaction const&) = default;
  Transaction& operator=(Transaction const&) = default;
  ///@}

  /// @name Equality operators
  ///@{
  friend bool operator==(Transaction const& a, Transaction const& b) {
    return a.impl_ == b.impl_;
  }
  friend bool operator!=(Transaction const& a, Transaction const& b) {
    return !(a == b);
  }
  ///@}

 private:
  // Friendship for access by internal helpers.
  template <typename T>
  friend Transaction internal::MakeSingleUseTransaction(T&&);
  template <typename Functor>
  friend internal::VisitInvokeResult<Functor> internal::Visit(Transaction,
                                                              Functor&&);
  friend Transaction internal::MakeTransactionFromIds(
      std::string session_id, std::string transaction_id);

  // Construction of a single-use transaction.
  explicit Transaction(SingleUseOptions opts);
  // Construction of a transaction with existing IDs.
  Transaction(std::string session_id, std::string transaction_id);

  std::shared_ptr<internal::TransactionImpl> impl_;
};

/**
 * Create a read-only transaction configured with @p opts.
 *
 * @copydoc Transaction::Transaction(ReadOnlyOptions)
 */
inline Transaction MakeReadOnlyTransaction(
    Transaction::ReadOnlyOptions opts = {}) {
  return Transaction(std::move(opts));
}

/**
 * Create a read-write transaction configured with @p opts.
 *
 * @copydoc Transaction::Transaction(ReadOnlyOptions)
 */
inline Transaction MakeReadWriteTransaction(
    Transaction::ReadWriteOptions opts = {}) {
  return Transaction(std::move(opts));
}

/**
 * Create a read-write transaction configured with @p opts, and sharing
 * lock priority with @p txn. This should be used when rerunning an aborted
 * transaction, so that the new attempt has a slightly better chance of
 * success.
 */
inline Transaction MakeReadWriteTransaction(
    Transaction const& txn, Transaction::ReadWriteOptions opts = {}) {
  return Transaction(txn, std::move(opts));
}

namespace internal {

template <typename T>
Transaction MakeSingleUseTransaction(T&& opts) {
  // Requires that `opts` is implicitly convertible to SingleUseOptions.
  Transaction::SingleUseOptions su_opts = std::forward<T>(opts);
  return Transaction(std::move(su_opts));
}

template <typename Functor>
// Pass `txn` by value, despite being used only once. This avoids the
// possibility of `txn` being destroyed by `f` before `Visit()` can
// return. Therefore, ...
// NOLINTNEXTLINE(performance-unnecessary-value-param)
VisitInvokeResult<Functor> Visit(Transaction txn, Functor&& f) {
  return txn.impl_->Visit(std::forward<Functor>(f));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TRANSACTION_H
