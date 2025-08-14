// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_SAMPLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_SAMPLER_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/row_key_sample.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/options.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Objects of this class represent the state of receiving row keys via
 * AsyncSampleRows.
 */
class AsyncRowSampler : public std::enable_shared_from_this<AsyncRowSampler> {
 public:
  static future<StatusOr<std::vector<bigtable::RowKeySample>>> Create(
      CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
      std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy, bool enable_server_retries,
      std::string const& app_profile_id, std::string const& table_name,
      std::shared_ptr<OperationContext> operation_context);

 private:
  AsyncRowSampler(CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
                  std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
                  std::unique_ptr<BackoffPolicy> backoff_policy,
                  bool enable_server_retries, std::string const& app_profile_id,
                  std::string const& table_name,
                  std::shared_ptr<OperationContext> operation_context);

  void StartIteration();
  future<bool> OnRead(google::bigtable::v2::SampleRowKeysResponse response);
  void OnFinish(Status const& status);

  CompletionQueue cq_;
  std::shared_ptr<BigtableStub> stub_;
  std::unique_ptr<bigtable::DataRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  bool enable_server_retries_;
  std::string app_profile_id_;
  std::string table_name_;

  std::atomic<bool> keep_reading_{true};
  std::vector<bigtable::RowKeySample> samples_;
  promise<StatusOr<std::vector<bigtable::RowKeySample>>> promise_;
  internal::ImmutableOptions options_;
  internal::CallContext call_context_;
  std::shared_ptr<grpc::ClientContext> client_context_;
  std::shared_ptr<OperationContext> operation_context_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_SAMPLER_H
