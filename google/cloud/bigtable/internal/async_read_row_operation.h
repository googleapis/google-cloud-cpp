// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_READ_ROW_OPERATION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_READ_ROW_OPERATION_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/async_row_reader.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Perform an AsyncReadRows operation request with retries.
 *
 * @tparam Client the class implementing the asynchronous operation, examples
 *     include `DataClient`, `AdminClient`, and `InstanceAdminClient`.
 *
 * @tparam ReadRowCallback the type of the function-like object that will
 * receive the results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, Row, grpc::Status&>);
 *
 * @tparam DoneCallback the type of the function-like object that will receive
 * the results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, bool&, grpc::Status&>);
 *
 * @tparam valid_data_callback_type a format parameter, uses `std::enable_if<>`
 * to disable this template if the ReadRowCallback functor does not match the
 * expected signature.
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the DoneCallback functor does not match the
 * expected signature.
 */
template <typename ReadRowCallback, typename DoneCallback,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  ReadRowCallback, CompletionQueue&, Row, grpc::Status&>::value,
              int>::type valid_data_callback_type = 0,
          typename std::enable_if<google::cloud::internal::is_invocable<
                                      DoneCallback, CompletionQueue&, bool&,
                                      grpc::Status const&>::value,
                                  int>::type valid_callback_type = 0>
class AsyncReadRowsOperation
    : public AsyncRetryOp<ConstantIdempotencyPolicy, DoneCallback,
                          AsyncRowReader<ReadRowCallback>> {
 public:
  AsyncReadRowsOperation(
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::shared_ptr<bigtable::DataClient> client,
      bigtable::AppProfileId const& app_profile_id,
      bigtable::TableId const& table_name, RowSet row_set,
      std::int64_t rows_limit, Filter filter, bool raise_on_error,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory,
      ReadRowCallback&& read_row_callback, DoneCallback&& done_callback)
      : AsyncRetryOp<ConstantIdempotencyPolicy, DoneCallback,
                     AsyncRowReader<ReadRowCallback>>(
            __func__, std::move(rpc_retry_policy),
            std::move(rpc_backoff_policy), ConstantIdempotencyPolicy(true),
            std::move(metadata_update_policy),
            std::forward<DoneCallback>(done_callback),
            AsyncRowReader<ReadRowCallback>(
                client, std::move(app_profile_id), std::move(table_name),
                std::move(row_set), rows_limit, std::move(filter),
                raise_on_error, std::move(parser_factory),
                std::move(read_row_callback))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_READ_ROW_OPERATION_H_
