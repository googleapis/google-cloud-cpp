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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TRANSACTION_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TRANSACTION_H_

#include "google/cloud/spanner/internal/transaction_impl.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/v1/transaction.pb.h>
#include <chrono>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class Transaction;  // defined below

// Internal forward declarations to befriend.
namespace internal {
template <typename T>
Transaction MakeSingleUseTransaction(T&&);
template <typename T>
T Visit(Transaction,
        std::function<T(google::spanner::v1::TransactionSelector&)>);
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
 *     requiring the application to retry.
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
    ReadOnlyOptions(Timestamp read_timestamp);

    // Exact Staleness: Executes all reads at a timestamp `exact_staleness`
    // old. The actual timestamp is chosen soon after the reads are started.
    ReadOnlyOptions(std::chrono::nanoseconds exact_staleness);

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
   * Client::Read() and Client::ExecuteSql(). SingleUseOptions cannot be used
   * to construct an application-level Transaction.
   */
  class SingleUseOptions {
   public:
    // Strong or Exact Staleness: See ReadOnlyOptions.
    SingleUseOptions(ReadOnlyOptions opts);  // implicitly convertible

    // Bounded Staleness: Executes all reads at a timestamp that is not
    // before `min_read_timestamp`.
    SingleUseOptions(Timestamp min_read_timestamp);

    // Bounded Staleness: Executes all reads at a timestamp that is not
    // before `NOW - max_staleness`.
    SingleUseOptions(std::chrono::nanoseconds max_staleness);

   private:
    friend Transaction;
    google::spanner::v1::TransactionOptions_ReadOnly ro_opts_;
  };

  /// @name Construction of read-only and read-write transactions.
  ///@{
  explicit Transaction(ReadOnlyOptions opts);
  explicit Transaction(ReadWriteOptions opts);
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
  template <typename T>
  friend T internal::Visit(
      Transaction, std::function<T(google::spanner::v1::TransactionSelector&)>);

  // Construction of a single-use transaction.
  explicit Transaction(SingleUseOptions opts);

  std::shared_ptr<internal::TransactionImpl> impl_;
};

Transaction MakeReadOnlyTransaction(Transaction::ReadOnlyOptions opts = {});
Transaction MakeReadWriteTransaction(Transaction::ReadWriteOptions opts = {});

namespace internal {

template <typename T>
Transaction MakeSingleUseTransaction(T&& opts) {
  // Requires that `opts` is implicitly convertible to SingleUseOptions.
  Transaction::SingleUseOptions su_opts = std::forward<T>(opts);
  return Transaction(std::move(su_opts));
}

template <typename T>
T Visit(Transaction txn,
        std::function<T(google::spanner::v1::TransactionSelector&)> f) {
  return txn.impl_->Visit(f);
}

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TRANSACTION_H_
