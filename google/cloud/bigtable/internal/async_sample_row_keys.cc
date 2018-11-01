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

#include "google/cloud/bigtable/internal/async_sample_row_keys.h"
#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include <numeric>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

AsyncSampleRowKeys::AsyncSampleRowKeys(
    std::shared_ptr<DataClient> client,
    bigtable::AppProfileId const& app_profile_id,
    bigtable::TableId const& table_name)
    : client_(std::move(client)) {
  bigtable::internal::SetCommonTableOperationRequest<
      google::bigtable::v2::SampleRowKeysRequest>(
      request_, app_profile_id.get(), table_name.get());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
