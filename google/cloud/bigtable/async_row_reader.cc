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

#include "google/cloud/bigtable/async_row_reader.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
AsyncRowReader::AsyncRowReader(
    CompletionQueue cq, std::shared_ptr<DataClient> client,
    bigtable::AppProfileId app_profile_id, bigtable::TableId table_name,
    RowSet row_set, std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
    : cq_(std::move(cq)),
      client_(std::move(client)),
      app_profile_id_(std::move(app_profile_id)),
      table_name_(std::move(table_name)),
      row_set_(std::move(row_set)),
      rows_limit_(rows_limit),
      filter_(std::move(filter)),
      rpc_retry_policy_(std::move(rpc_retry_policy)),
      rpc_backoff_policy_(std::move(rpc_backoff_policy)),
      metadata_update_policy_(std::move(metadata_update_policy)),
      parser_factory_(std::move(parser_factory)),
      rows_count_(0) {}

AsyncRowReader::~AsyncRowReader() {
  assert(whole_op_finished_);
  assert(promised_results_.empty());
  assert(!continue_reading_.has_value());
}

std::shared_ptr<AsyncRowReader> AsyncRowReader::Create(
    CompletionQueue cq, std::shared_ptr<DataClient> client,
    bigtable::AppProfileId app_profile_id, bigtable::TableId table_name,
    RowSet row_set, std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory) {
  std::shared_ptr<AsyncRowReader> res(new AsyncRowReader(
      std::move(cq), std::move(client), std::move(app_profile_id),
      std::move(table_name), std::move(row_set), rows_limit, std::move(filter),
      std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
      std::move(metadata_update_policy), std::move(parser_factory)));
  res->MakeRequest();
  return res;
}

future<AsyncRowReader::Response> AsyncRowReader::Next() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!ready_rows_.empty()) {
    auto res = make_ready_future<Response>(
        optional<Row>(std::move(ready_rows_.front())));
    ready_rows_.pop();
    return res;
  }
  if (whole_op_finished_) {
    if (whole_op_finished_->ok()) {
      return make_ready_future<Response>(optional<Row>());
    }
    return make_ready_future<Response>(*whole_op_finished_);
  }
  promised_results_.emplace(promise<Response>());
  auto res = promised_results_.back().get_future();
  if (continue_reading_) {
    // If `AsyncRowReader` was waiting to read more data, trigger it.
    optional<promise<bool>> continue_reading = std::move(continue_reading_);
    continue_reading_ = optional<promise<bool>>();
    lk.unlock();
    continue_reading->set_value(true);
  }
  return res;
}

void AsyncRowReader::MakeRequest() {
  std::unique_lock<std::mutex> lk(mu_);
  stream_res_override_ = Status();
  google::bigtable::v2::ReadRowsRequest request;

  request.set_app_profile_id(app_profile_id_.get());
  request.set_table_name(table_name_.get());
  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }
  parser_ = parser_factory_->Create();

  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  rpc_retry_policy_->Setup(*context);
  rpc_backoff_policy_->Setup(*context);
  metadata_update_policy_.Setup(*context);

  auto self = shared_from_this();
  auto client = client_;
  cq_.MakeStreamingReadRpc(
      [client](grpc::ClientContext* context,
               google::bigtable::v2::ReadRowsRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->PrepareAsyncReadRows(context, request, cq);
      },
      std::move(request), std::move(context),
      [self](google::bigtable::v2::ReadRowsResponse r) {
        return self->OnDataReceived(std::move(r));
      },
      [self](Status s) { self->OnStreamFinished(std::move(s)); });
}

future<bool> AsyncRowReader::OnDataReceived(
    google::bigtable::v2::ReadRowsResponse response) {
  std::unique_lock<std::mutex> lk(mu_);
  stream_res_override_ = ConsumeResponse(std::move(response));
  // We've processed the response. Even if stream_res_override_ is not OK, we
  // might have consumed some rows, so we might be able to satisfy some promises
  // made to the user.
  //
  // It is crucial to make sure that we do it _before_ we send a new request not
  // to accidentally reorder incoming results.
  //
  // In order to achieve it, we defer storing a continue_reading_ promise to the
  // end of this function.
  std::vector<std::pair<promise<Response>, Row>> to_satisfy;
  while (!promised_results_.empty() && !ready_rows_.empty()) {
    to_satisfy.emplace_back(std::make_pair(std::move(promised_results_.front()),
                                           std::move(ready_rows_.front())));
    promised_results_.pop();
    ready_rows_.pop();
  }

  lk.unlock();
  for (auto& promise_and_row : to_satisfy) {
    promise_and_row.first.set_value(
        optional<Row>(std::move(promise_and_row.second)));
  }
  lk.lock();

  if (!stream_res_override_.ok()) {
    return make_ready_future<bool>(false);
  }
  if (!promised_results_.empty()) {
    return make_ready_future<bool>(true);
  }
  continue_reading_.emplace(promise<bool>());
  return continue_reading_->get_future();
}

void AsyncRowReader::OnStreamFinished(Status status) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!stream_res_override_.ok()) {
    status = stream_res_override_;
  }
  grpc::Status parser_status;
  parser_->HandleEndOfStream(parser_status);
  if (!parser_status.ok() && status.ok()) {
    // If there stream finished with an error ignore what the parser says.
    status = internal::MakeStatusFromRpcError(parser_status);
  }

  // In the unlikely case when we have already reached the requested
  // number of rows and still receive an error (the parser can throw
  // an error at end of stream for example), there is no need to
  // retry and we have no good value for rows_limit anyway.
  if (rows_limit_ != NO_ROWS_LIMIT && rows_limit_ <= rows_count_) {
    status = Status();
  }

  if (!last_read_row_key_.empty()) {
    // We've returned some rows and need to make sure we don't
    // request them again.
    row_set_ = row_set_.Intersect(RowRange::Open(last_read_row_key_, ""));
  }

  // If we receive an error, but the retriable set is empty, consider it a
  // success.
  if (row_set_.IsEmpty()) {
    status = Status();
  }

  if (status.ok()) {
    // We've successfuly finished the scan.
    OperationComplete(Status(), lk);  // unlocks the lock
    return;
  }

  if (!rpc_retry_policy_->OnFailure(status)) {
    // Can't retry.
    OperationComplete(status, lk);  // unlocks the lock
    return;
  }
  auto self = shared_from_this();
  cq_.MakeRelativeTimer(rpc_backoff_policy_->OnCompletion(status))
      .then([self](future<std::chrono::system_clock::time_point>) {
        self->MakeRequest();
      });
}

void AsyncRowReader::OperationComplete(Status status,
                                       std::unique_lock<std::mutex>& lk) {
  whole_op_finished_ = status;
  std::queue<promise<Response>> promised_results;
  promised_results.swap(promised_results_);
  lk.unlock();
  for (; !promised_results.empty(); promised_results.pop()) {
    auto& promise = promised_results.front();
    if (status.ok()) {
      promise.set_value(optional<Row>());
      return;
    }
    promise.set_value(status);
  }
}

Status AsyncRowReader::ConsumeFromParser() {
  grpc::Status status;
  while (parser_->HasNext()) {
    Row parsed_row = parser_->Next(status);
    if (!status.ok()) {
      return internal::MakeStatusFromRpcError(status);
    }
    ++rows_count_;
    last_read_row_key_ = std::string(parsed_row.row_key());
    ready_rows_.emplace(std::move(parsed_row));
  }
  return Status();
}

Status AsyncRowReader::ConsumeResponse(
    google::bigtable::v2::ReadRowsResponse response) {
  for (auto& chunk : *response.mutable_chunks()) {
    grpc::Status status;
    parser_->HandleChunk(std::move(chunk), status);
    if (!status.ok()) {
      return internal::MakeStatusFromRpcError(status);
    }
    Status parser_status = ConsumeFromParser();
    if (!parser_status.ok()) {
      return parser_status;
    }
  }
  return Status();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
