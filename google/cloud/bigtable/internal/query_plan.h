
// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H

#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class QueryPlan : public std::enable_shared_from_this<QueryPlan> {
 public:
  // Typically, a lambda capturing the original PrepareQueryRequest and
  // DataConnection pointer necessary to call the PrepareQuery RPC.
  using RefreshFn = std::function<google::bigtable::v2::PrepareQueryResponse()>;

  explicit QueryPlan(CompletionQueue const& cq,
                     google::bigtable::v2::PrepareQueryResponse const& response,
                     RefreshFn const& fn)
      : QueryPlan(PrivateConstructor{}, cq, response, fn) {}

  // Calls the constructor and then Initialize.
  static std::shared_ptr<QueryPlan> Create(
      CompletionQueue const& cq,
      google::bigtable::v2::PrepareQueryResponse const& response,
      RefreshFn const& fn) {
    return std::make_shared<QueryPlan>(cq, response, fn);
  }

  // Accessor for the prepared_query field in response_.
  std::string const& prepared_query() const;

  // Accessor for the metadata field in  response_.
  google::bigtable::v2::ResultSetMetadata const& metadata() const;

 private:
  struct PrivateConstructor {};
  QueryPlan(PrivateConstructor, CompletionQueue cq,
            google::bigtable::v2::PrepareQueryResponse response, RefreshFn fn)
      : cq_(std::move(cq)),
        response_(std::move(response)),
        fn_(std::move(fn)) {}

  // Performs the first call to ScheduleRefresh and any other initialization not
  // possible in the constructor.
  void Initialize() {}

  // Calls MakeDeadlineTimer on the CompletionQueue with a continuation lambda
  // capturing a std::weak_ptr to this that calls RefreshQueryPlan.
  void ScheduleRefresh() {}

  // Performs the synchronization around calling RefreshFn and updating
  // response_.
  void RefreshQueryPlan() {}

  CompletionQueue cq_;
  future<void> refresh_timer_;
  mutable std::mutex mu_;
  std::condition_variable cond_;
  google::bigtable::v2::PrepareQueryResponse response_;  // GUARDED_BY(mu_)
  RefreshFn fn_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H
