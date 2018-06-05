// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATIONS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATIONS_H_

#include "google/cloud/bigtable/internal/conjunction.h"
#include "google/cloud/bigtable/internal/endian.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <type_traits>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Represent a single change to a specific row in a Table.
 *
 * Mutations come in different forms, they can set a specific cell,
 * delete a specific cell or delete multiple cells in a row.
 */
struct Mutation {
  google::bigtable::v2::Mutation op;
};

/// Create a mutation to set a cell value.
Mutation SetCell(std::string family, std::string column,
                 std::chrono::milliseconds timestamp, std::string value);

/**
 * Create a mutation to set a cell value where the server sets the time.
 *
 * These mutations are not idempotent and not retried by default.
 */
Mutation SetCell(std::string family, std::string column, std::string value);

/**
 * A magic value where the server sets the timestamp.
 *
 * Notice that using this value in a SetCell() mutation makes it non-idempotent,
 * and by default the client will not retry such mutations.
 */
constexpr std::int64_t ServerSetTimestamp() { return -1; }

//@{
/**
 * @name Create mutations to delete a range of cells from a column.
 *
 * The following functions create a mutation that deletes all the
 * cells in the given column family and, column within the given
 * timestamp in the range.
 *
 * The function accepts any instantiation of `std::chrono::duration<>` for the
 * @p timestamp_begin and @p timestamp_end parameters.  For example:
 *
 * @code
 * using namespace std::chrono_literals; // C++14
 * bigtable::DeleteFromColumn("fam", "col", 0us, 10us)
 * @endcode
 *
 * The ending timestamp is exclusive, while the beginning timestamp is
 * inclusive.  That is, the interval is [@p timestamp_begin, @p timestamp_end).
 * The value 0 is special and treated as "unbounded" for both the begin and
 * end endpoints of the time range.  The Cloud Bigtable server rejects
 * invalid and empty ranges, i.e., any range where the endpoint is smaller or
 * equal than to the initial endpoint unless either endpoint is 0.
 *
 * @tparam Rep1 a placeholder to match the Rep tparam for @p timestamp_begin
 * type. The semantics of this template parameter are documented in
 * std::chrono::duration<>` (in brief, the underlying arithmetic type
 * used to store the number of ticks), for our purposes it is simply a
 * formal parameter.
 *
 * @tparam Rep2 similar formal parameter for the type of @p timestamp_end.
 *
 * @tparam Period1 a placeholder to match the Period tparam for
 * @p timestamp_begin type. The semantics of this template parameter are
 * documented in `std::chrono::duration<>` (in brief, the length of the tick
 * in seconds,vexpressed as a `std::ratio<>`), for our purposes it is simply
 * a formal parameter.
 *
 * @tparam Period2 similar formal parameter for the type of @p timestamp_end.
 */
template <typename Rep1, typename Period1, typename Rep2, typename Period2>
Mutation DeleteFromColumn(std::string family, std::string column,
                          std::chrono::duration<Rep1, Period1> timestamp_begin,
                          std::chrono::duration<Rep2, Period2> timestamp_end) {
  Mutation m;
  using namespace std::chrono;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  d.mutable_time_range()->set_start_timestamp_micros(
      duration_cast<microseconds>(timestamp_begin).count());
  d.mutable_time_range()->set_end_timestamp_micros(
      duration_cast<microseconds>(timestamp_end).count());
  return m;
}

//@{
/**
 * @name The following functions create a mutation that deletes all the
 * cells in the given column family and column, starting from and
 * including, @a timestamp_begin.
 *
 * The function accepts any instantiation of `std::chrono::duration<>` for the
 * @p timestamp_begin For example:
 *
 * @code
 * using namespace std::chrono_literals; // C++14
 * bigtable::DeleteFromColumn("fam", "col", 10us)
 * @endcode
 *
 * @tparam Rep1 a placeholder to match the Rep tparam for @p timestamp_begin
 * type. The semantics of this template parameter are documented in
 * `std::chrono::duration<>` (in brief, the underlying arithmetic type
 * used to store the number of ticks), for our purposes it is simply a
 * formal parameter.
 *
 * @tparam Period1 a placeholder to match the Period tparam for @p
 * timestamp_begin type. The semantics of this template parameter
 * are documented in `std::chrono::duration<>` (in brief, the length
 * of the tick in seconds, expressed as a `std::ratio<>`), for our
 * purposes it is simply a formal parameter.
 */
template <typename Rep1, typename Period1>
Mutation DeleteFromColumnStartingFrom(
    std::string family, std::string column,
    std::chrono::duration<Rep1, Period1> timestamp_begin) {
  Mutation m;
  using namespace std::chrono;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  d.mutable_time_range()->set_start_timestamp_micros(
      duration_cast<microseconds>(timestamp_begin).count());
  return m;
}

//@{
/**
 * @name The following functions create a mutation that deletes all the
 * cells in the given column family and column, Delete up to @a timestamp_end,
 * but excluding, @a timestamp_end.
 *
 * The function accepts any instantiation of `std::chrono::duration<>` for the
 * @p timestamp_end For example:
 *
 * @code
 * using namespace std::chrono_literals; // C++14
 * bigtable::DeleteFromColumn("fam", "col", 10us)
 * @endcode
 *
 * @tparam Rep2 a placeholder to match the Rep tparam for @p timestamp_end type.
 * The semantics of this template parameter are documented in
 * `std::chrono::duration<>` (in brief, the underlying arithmetic type
 * used to store the number of ticks), for our purposes it is simply a
 * formal parameter.
 *
 * @tparam Period2 a placeholder to match the Period tparam for @p timestamp_end
 * type. The semantics of this template parameter are documented in
 * `std::chrono::duration<>` (in brief, the length of the tick in seconds,
 * expressed as a `std::ratio<>`), for our purposes it is simply a formal
 * parameter.
 */
template <typename Rep2, typename Period2>
Mutation DeleteFromColumnEndingAt(
    std::string family, std::string column,
    std::chrono::duration<Rep2, Period2> timestamp_end) {
  Mutation m;
  using namespace std::chrono;
  auto& d = *m.op.mutable_delete_from_column();
  d.set_family_name(std::move(family));
  d.set_column_qualifier(std::move(column));
  d.mutable_time_range()->set_end_timestamp_micros(
      duration_cast<microseconds>(timestamp_end).count());
  return m;
}

/// Delete all the values for the column.
Mutation DeleteFromColumn(std::string family, std::string column);
//@}

/// Create a mutation to delete all the cells in a column family.
Mutation DeleteFromFamily(std::string family);

/// Create a mutation to delete all the cells in a row.
Mutation DeleteFromRow();

/**
 * Represent a single row mutation.
 *
 * Bigtable can perform multiple changes to a single row atomically.
 * This class represents 0 or more changes to apply to a single row.
 * The changes may include setting cells (which implicitly insert the
 * values), deleting values, etc.
 */
class SingleRowMutation {
 public:
  /// Create an empty mutation.
  explicit SingleRowMutation(std::string row_key)
      : row_key_(std::move(row_key)) {}

  /// Create a row mutation from a initializer list.
  SingleRowMutation(std::string row_key, std::initializer_list<Mutation> list)
      : row_key_(std::move(row_key)) {
    for (auto&& i : list) {
      *ops_.Add() = i.op;
    }
  }

  /// Create a single-row multiple-cell mutation from a variadic list.
  template <typename... M>
  explicit SingleRowMutation(std::string row_key, M&&... m)
      : row_key_(std::move(row_key)) {
    static_assert(
        internal::conjunction<std::is_convertible<M, Mutation>...>::value,
        "The arguments passed to SingleRowMutation(std::string, ...) must be "
        "convertible to Mutation");
    emplace_many(std::forward<M>(m)...);
  }

  /// Create a row mutation from gRPC proto
  explicit SingleRowMutation(
      ::google::bigtable::v2::MutateRowsRequest::Entry&& entry)
      : row_key_(std::move(*entry.mutable_row_key())), ops_() {
    ops_.Swap(entry.mutable_mutations());
  }

  /// Create a row mutation from gRPC proto
  explicit SingleRowMutation(::google::bigtable::v2::MutateRowRequest&& request)
      : row_key_(std::move(*request.mutable_row_key())), ops_() {
    ops_.Swap(request.mutable_mutations());
  }

  // Add a mutation at the end.
  SingleRowMutation& emplace_back(Mutation&& mut) {
    ops_.Add()->Swap(&mut.op);
    return *this;
  }

  // Get the row key.
  std::string const& row_key() const { return row_key_; }

  friend class Table;

  SingleRowMutation(SingleRowMutation&& rhs) = default;
  SingleRowMutation& operator=(SingleRowMutation&& rhs) = default;
  SingleRowMutation(SingleRowMutation const& rhs) = default;
  SingleRowMutation& operator=(SingleRowMutation const& rhs) = default;

  /// Move the contents into a bigtable::v2::MutateRowsRequest::Entry.
  void MoveTo(google::bigtable::v2::MutateRowsRequest::Entry* entry) {
    entry->set_row_key(std::move(row_key_));
    entry->mutable_mutations()->Swap(&ops_);
  }

  /// Transfer the contents to @p request.
  void MoveTo(google::bigtable::v2::MutateRowRequest& request) {
    request.set_row_key(std::move(row_key_));
    request.mutable_mutations()->Swap(&ops_);
  }

 private:
  /// Add multiple mutations to single row
  template <typename... M>
  void emplace_many(Mutation&& first, M&&... tail) {
    emplace_back(std::forward<Mutation>(first));
    emplace_many(std::forward<M>(tail)...);
  }

  void emplace_many(Mutation&& m) { emplace_back(std::forward<Mutation>(m)); }

 private:
  std::string row_key_;
  google::protobuf::RepeatedPtrField<google::bigtable::v2::Mutation> ops_;
};

/**
 * A SingleRowMutation that failed.
 *
 * A multi-row mutation returns the list of operations that failed,
 * this class encapsulates both the failure and the original
 * mutation.  The application can then choose to resend the mutation,
 * or log it, or save it for processing via some other means.
 */
class FailedMutation {
 public:
  FailedMutation(SingleRowMutation mut, google::rpc::Status status)
      : FailedMutation(std::move(mut), std::move(status), -1) {}
  FailedMutation(SingleRowMutation mut, google::rpc::Status status, int index)
      : mutation_(std::move(mut)),
        status_(ToGrpcStatus(status)),
        original_index_(index) {}

  FailedMutation(FailedMutation&&) = default;
  FailedMutation& operator=(FailedMutation&&) = default;
  FailedMutation(FailedMutation const&) = default;
  FailedMutation& operator=(FailedMutation const&) = default;

  //@{
  /// @name accessors
  SingleRowMutation const& mutation() const { return mutation_; }
  grpc::Status const& status() const { return status_; }
  int original_index() const { return original_index_; }
  //@}

  friend class BulkMutation;

 private:
  static grpc::Status ToGrpcStatus(google::rpc::Status const& status);

 private:
  SingleRowMutation mutation_;
  grpc::Status status_;
  int original_index_;
};

/**
 * Report unrecoverable errors in a partially completed mutation.
 */
class PermanentMutationFailure : public std::runtime_error {
 public:
  PermanentMutationFailure(char const* msg,
                           std::vector<FailedMutation>&& failures)
      : std::runtime_error(msg), failures_(std::move(failures)) {}

  PermanentMutationFailure(char const* msg, grpc::Status status,
                           std::vector<FailedMutation>&& failures)
      : std::runtime_error(msg),
        failures_(std::move(failures)),
        status_(std::move(status)) {}

  /**
   * The details of each mutation failure.
   *
   * Because BulkApply() and Apply() take ownership of the data in the mutations
   * the failures are returned with their full contents, in case the application
   * wants to take further action with them.  Any successful mutations are
   * discarded.
   *
   * Any mutations that fail with an unknown state are included with a
   * grpc::StatusCode::OK.
   */
  std::vector<FailedMutation> const& failures() const { return failures_; }

  /**
   * The grpc::Status of the request.
   *
   * Notice that it can return grpc::Status::OK when there are partial failures
   * in a BulkApply() operation.
   */
  grpc::Status const& status() const { return status_; }

 private:
  std::vector<FailedMutation> failures_;
  grpc::Status status_;
};

/**
 * Represent a set of mutations across multiple rows.
 *
 * Cloud Bigtable can batch multiple mutations in a single request.
 * The mutations are not atomic, but it is more efficient to send them
 * in a batch than to make multiple smaller requests.
 */
class BulkMutation {
 public:
  /// Create an empty set of mutations.
  BulkMutation() : request_() {}

  /// Create a multi-row mutation from a range of SingleRowMutations.
  template <typename iterator>
  BulkMutation(iterator begin, iterator end) {
    static_assert(
        std::is_convertible<decltype(*begin), SingleRowMutation>::value,
        "The iterator value type must be convertible to SingleRowMutation");
    for (auto i = begin; i != end; ++i) {
      push_back(*i);
    }
  }

  /// Create a multi-row mutation from a initializer list.
  BulkMutation(std::initializer_list<SingleRowMutation> list)
      : BulkMutation(list.begin(), list.end()) {}

  /// Create a muti-row mutation from a SingleRowMutation
  explicit BulkMutation(SingleRowMutation&& mutation) : BulkMutation() {
    emplace_back(std::move(mutation));
  }

  /// Create a muti-row mutation from two SingleRowMutation
  BulkMutation(SingleRowMutation&& m1, SingleRowMutation&& m2)
      : BulkMutation() {
    emplace_back(std::move(m1));
    emplace_back(std::move(m2));
  }

  /// Create a muti-row mutation from a variadic list.
  template <typename... M>
  BulkMutation(M&&... m) : BulkMutation() {
    static_assert(
        internal::conjunction<
            std::is_convertible<M, SingleRowMutation>...>::value,
        "The arguments passed to BulkMutation(...) must be convertible"
        " to SingleRowMutation");
    emplace_many(std::forward<M>(m)...);
  }

  // Add a mutation to the batch.
  BulkMutation& emplace_back(SingleRowMutation&& mut) {
    mut.MoveTo(request_.add_entries());
    return *this;
  }

  // Add a failed mutation to the batch.
  BulkMutation& emplace_back(FailedMutation&& fm) {
    fm.mutation_.MoveTo(request_.add_entries());
    fm.status_ = grpc::Status::OK;
    return *this;
  }

  // Add a mutation to the batch.
  BulkMutation& push_back(SingleRowMutation mut) {
    mut.MoveTo(request_.add_entries());
    return *this;
  }

  /// Move the contents into a bigtable::v2::MutateRowsRequest
  void MoveTo(google::bigtable::v2::MutateRowsRequest* request) {
    request_.Swap(request);
    request_ = {};
  }

  /// Return true if there are no mutations in this set.
  bool empty() const { return request_.entries().empty(); }

 private:
  template <typename... M>
  void emplace_many(SingleRowMutation&& first, M&&... tail) {
    emplace_back(std::forward<SingleRowMutation>(first));
    emplace_many(std::forward<M>(tail)...);
  }

  void emplace_many(SingleRowMutation&& m) {
    emplace_back(std::forward<SingleRowMutation>(m));
  }

 private:
  google::bigtable::v2::MutateRowsRequest request_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATIONS_H_
