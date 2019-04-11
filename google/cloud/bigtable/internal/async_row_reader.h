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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_READER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_READER_H_

#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/readrowsparser.h"
#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/bigtable/version.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <cinttypes>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Async-friendly version RowReader.
 *
 * It satisfies the requirements to be used in `AsyncRetryOp`.
 */
template <typename ReadRowCallback,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  ReadRowCallback, CompletionQueue&, Row, grpc::Status&>::value,
              int>::type valid_data_callback_type = 0>
class AsyncRowReader {
 public:
  /**
   * A constant for the magic value that means "no limit, get all rows".
   *
   * Zero is used as a magic value that means "get all rows" in the
   * Cloud Bigtable RPC protocol.
   */
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;

  AsyncRowReader(
      std::shared_ptr<bigtable::DataClient> client,
      bigtable::AppProfileId const& app_profile_id,
      bigtable::TableId const& table_name, RowSet row_set,
      std::int64_t rows_limit, Filter filter, bool raise_on_error,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory,
      ReadRowCallback read_row_callback)
      : client_(std::move(client)),
        app_profile_id_(std::move(app_profile_id)),
        table_name_(std::move(table_name)),
        row_set_(std::move(row_set)),
        rows_limit_(rows_limit),
        filter_(std::move(filter)),
        context_(),
        parser_factory_(std::move(parser_factory)),
        parser_(parser_factory_->Create()),
        rows_count_(0),
        status_(grpc::Status::OK),
        read_row_callback_(read_row_callback) {}

  using Request = google::bigtable::v2::ReadRowsRequest;
  using Response = bool;

  void ProcessResponse(CompletionQueue& cq,
                       google::bigtable::v2::ReadRowsResponse& response) {
    int processed_chunks_count_ = 0;
    while (processed_chunks_count_ < response.chunks_size()) {
      parser_->HandleChunk(
          std::move(*(response.mutable_chunks(processed_chunks_count_))),
          status_);
      if (!status_.ok()) {
        // An error must result in a retry, so we return without calling the
        // callback function and check for status before finishing the call
        return;
      }

      if (parser_->HasNext()) {
        // We have a complete row in the parser.
        Row parsed_row = parser_->Next(status_);
        if (!status_.ok()) {
          return;
        }
        ++rows_count_;
        last_read_row_key_ = std::string(parsed_row.row_key());
        read_row_callback_(cq, std::move(parsed_row), status_);
      }
      ++processed_chunks_count_;
    }
  }

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      Functor&& callback) {
    google::bigtable::v2::ReadRowsRequest request;
    request.set_app_profile_id(app_profile_id_.get());
    request.set_table_name(table_name_.get());

    if (!last_read_row_key_.empty()) {
      // We've returned some rows and need to make sure we don't
      // request them again.
      row_set_ = row_set_.Intersect(RowRange::Open(last_read_row_key_, ""));
    }
    auto row_set_proto = row_set_.as_proto();
    request.mutable_rows()->Swap(&row_set_proto);

    auto filter_proto = filter_.as_proto();
    request.mutable_filter()->Swap(&filter_proto);

    if (rows_limit_ != NO_ROWS_LIMIT) {
      request.set_rows_limit(rows_limit_ - rows_count_);
    }
    context_ = google::cloud::internal::make_unique<grpc::ClientContext>();

    return cq.MakeUnaryStreamRpc(
        *client_, &DataClient::AsyncReadRows, request, std::move(context),
        [this](CompletionQueue& cq, const grpc::ClientContext& context,
               google::bigtable::v2::ReadRowsResponse& response) {
          ProcessResponse(cq, response);
        },
        FinishedCallback<Functor>(*this, std::forward<Functor>(callback)));
  }

  bool AccumulatedResult() { return status_.ok(); }

 private:
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  struct FinishedCallback {
    FinishedCallback(AsyncRowReader& parent, Functor callback)
        : parent_(parent), callback_(std::move(callback)) {}

    void operator()(CompletionQueue& cq, grpc::ClientContext& context,
                    grpc::Status& status) {
      if (status.ok() && parent_.status_.ok()) {
        // a successful call so close the parser.
        parent_.parser_->HandleEndOfStream(status);
      }

      if (!parent_.status_.ok() && status.ok()) {
        status = grpc::Status(grpc::StatusCode::UNAVAILABLE,
                              "Some rows were not returned");
      }

      callback_(cq, status);
    }

    // The user of AsyncRowReader has to make sure that it is not destructed
    // before all callbacks return, so we have a guarantee that this reference
    // is valid for as long as we don't call callback_.
    AsyncRowReader& parent_;
    Functor callback_;
  };

 private:
  std::shared_ptr<bigtable::DataClient> client_;
  bigtable::AppProfileId app_profile_id_;
  bigtable::TableId table_name_;
  RowSet row_set_;
  std::int64_t rows_limit_;
  Filter filter_;
  std::unique_ptr<grpc::ClientContext> context_;

  std::unique_ptr<internal::ReadRowsParserFactory> parser_factory_;
  std::unique_ptr<internal::ReadRowsParser> parser_;

  /// Number of rows read so far, used to set row_limit in retries.
  std::int64_t rows_count_;
  /// Holds the last read row key, for retries.
  std::string last_read_row_key_;

  grpc::Status status_;
  ReadRowCallback read_row_callback_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_READER_H_
