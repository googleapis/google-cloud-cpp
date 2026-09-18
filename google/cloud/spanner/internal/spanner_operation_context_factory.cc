// Copyright 2026 Google LLC
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

#include "google/cloud/spanner/internal/spanner_operation_context_factory.h"
#include "google/cloud/spanner/internal/spanner_request_id.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DefaultSpannerOperationContextFactory::DefaultSpannerOperationContextFactory(
    std::uint64_t client_id,
    std::shared_ptr<std::string const> process_random_id)
    : client_id_(client_id),
      process_random_id_(std::move(process_random_id)),
      static_prefix_(std::make_shared<std::string const>(
          FormatSpannerRequestStaticPrefix(1, *process_random_id_, client_id_)))
#ifndef _WIN32
      ,
      cached_pid_(getpid())
#endif
{
}

std::shared_ptr<std::string const>
DefaultSpannerOperationContextFactory::StaticPrefix() {
#ifndef _WIN32
  pid_t const current_pid = getpid();
  std::scoped_lock lock(mu_);
  if (current_pid != cached_pid_) {
    process_random_id_ = std::make_shared<std::string const>(ProcessRandomId());
    static_prefix_ = std::make_shared<std::string const>(
        FormatSpannerRequestStaticPrefix(1, *process_random_id_, client_id_));
    cached_pid_ = current_pid;
  }
  return static_prefix_;
#else
  return static_prefix_;
#endif
}

std::uint64_t DefaultSpannerOperationContextFactory::NextUserRequestIndex() {
  return next_user_request_index_.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t
DefaultSpannerOperationContextFactory::NextBackgroundRequestIndex() {
  return next_background_request_index_.fetch_add(1, std::memory_order_relaxed);
}

OperationContext DefaultSpannerOperationContextFactory::CreateSession() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "CreateSession");
}

OperationContext DefaultSpannerOperationContextFactory::BatchCreateSessions() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "BatchCreateSessions");
}

OperationContext DefaultSpannerOperationContextFactory::GetSession() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(), "GetSession");
}

OperationContext DefaultSpannerOperationContextFactory::ListSessions() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "ListSessions");
}

OperationContext DefaultSpannerOperationContextFactory::DeleteSession() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "DeleteSession");
}

OperationContext DefaultSpannerOperationContextFactory::ExecuteSql() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(), "ExecuteSql");
}

OperationContext DefaultSpannerOperationContextFactory::ExecuteStreamingSql() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "ExecuteStreamingSql");
}

OperationContext DefaultSpannerOperationContextFactory::ExecuteBatchDml() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "ExecuteBatchDml");
}

OperationContext DefaultSpannerOperationContextFactory::StreamingRead() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "StreamingRead");
}

OperationContext DefaultSpannerOperationContextFactory::BeginTransaction() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "BeginTransaction");
}

OperationContext DefaultSpannerOperationContextFactory::Commit() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(), "Commit");
}

OperationContext DefaultSpannerOperationContextFactory::Rollback() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(), "Rollback");
}

OperationContext DefaultSpannerOperationContextFactory::PartitionQuery() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "PartitionQuery");
}

OperationContext DefaultSpannerOperationContextFactory::PartitionRead() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(),
                          "PartitionRead");
}

OperationContext DefaultSpannerOperationContextFactory::BatchWrite() {
  return OperationContext(StaticPrefix(), NextUserRequestIndex(), "BatchWrite");
}

OperationContext
DefaultSpannerOperationContextFactory::BackgroundCreateSession() {
  return OperationContext(StaticPrefix(), NextBackgroundRequestIndex(),
                          "BackgroundCreateSession");
}

OperationContext
DefaultSpannerOperationContextFactory::BackgroundBatchCreateSessions() {
  return OperationContext(StaticPrefix(), NextBackgroundRequestIndex(),
                          "BackgroundBatchCreateSessions");
}

OperationContext
DefaultSpannerOperationContextFactory::BackgroundDeleteSession() {
  return OperationContext(StaticPrefix(), NextBackgroundRequestIndex(),
                          "BackgroundDeleteSession");
}

OperationContext
DefaultSpannerOperationContextFactory::BackgroundRefreshSession() {
  return OperationContext(StaticPrefix(), NextBackgroundRequestIndex(),
                          "BackgroundRefreshSession");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
