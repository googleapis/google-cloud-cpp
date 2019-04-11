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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_POLL_LONGRUNNING_OPERATION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_POLL_LONGRUNNING_OPERATION_H_

#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/version.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Implements the function for long running operation.
 */
template <typename ResultType, typename ClientType>
ResultType PollLongRunningOperation(
    std::shared_ptr<ClientType> client,
    std::unique_ptr<PollingPolicy> polling_policy,
    MetadataUpdatePolicy metadata_update_policy,
    google::longrunning::Operation& operation, char const* error_message,
    grpc::Status& status) {
  do {
    if (operation.done()) {
      if (operation.has_response()) {
        auto const& any = operation.response();
        if (!any.Is<ResultType>()) {
          std::ostringstream os;
          os << error_message << "(" << metadata_update_policy.value() << ") - "
             << "invalid result type in operation=" << operation.name();
          status = grpc::Status(grpc::StatusCode::UNKNOWN, os.str());
          return ResultType{};
        }
        ResultType result;
        any.UnpackTo(&result);
        return result;
      }
      if (operation.has_error()) {
        std::ostringstream os;
        os << error_message << "(" << metadata_update_policy.value() << ") - "
           << "error reported by operation=" << operation.name();
        status = grpc::Status(
            static_cast<grpc::StatusCode>(operation.error().code()),
            operation.error().message(), os.str());
        return ResultType{};
      }
    }
    auto delay = polling_policy->WaitPeriod();
    std::this_thread::sleep_for(delay);
    google::longrunning::GetOperationRequest op;
    op.set_name(operation.name());

    grpc::ClientContext context;
    polling_policy->Setup(context);

    status = client->GetOperation(&context, op, &operation);
    if (!status.ok() && !polling_policy->OnFailure(status)) {
      return ResultType{};
    }
  } while (!polling_policy->Exhausted());
  std::ostringstream os;
  os << error_message << "(" << metadata_update_policy.value() << ") - "
     << "polling policy exhausted in operation=" << operation.name();
  status = grpc::Status(grpc::StatusCode::UNKNOWN, os.str());
  return ResultType{};
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_POLL_LONGRUNNING_OPERATION_H_
