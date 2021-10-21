// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_SAMPLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_SAMPLER_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/row_key_sample.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future_generic.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Objects of this class represent the state of receiving row keys via
 * AsyncSampleRows.
 */
class AsyncRowSampler : public std::enable_shared_from_this<AsyncRowSampler> {
 public:
  static future<StatusOr<std::vector<RowKeySample>>> Create(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy, std::string app_profile_id,
      std::string table_name);

 private:
  AsyncRowSampler(CompletionQueue cq, std::shared_ptr<DataClient> client,
                  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                  MetadataUpdatePolicy metadata_update_policy,
                  std::string app_profile_id, std::string table_name);

  void StartIteration();
  future<bool> OnRead(google::bigtable::v2::SampleRowKeysResponse response);
  void OnFinish(Status const& status);

  CompletionQueue cq_;
  std::shared_ptr<DataClient> client_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::string app_profile_id_;
  std::string table_name_;

  bool stream_cancelled_;
  std::vector<RowKeySample> samples_;
  promise<StatusOr<std::vector<RowKeySample>>> promise_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_ROW_SAMPLER_H
