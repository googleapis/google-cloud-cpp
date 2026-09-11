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
      process_random_id_(
          process_random_id
              ? std::move(process_random_id)
              : std::make_shared<std::string const>(ProcessRandomId())),
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

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::CreateSession() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "CreateSession");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BatchCreateSessions() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "BatchCreateSessions");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::GetSession() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "GetSession");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::ListSessions() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "ListSessions");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::DeleteSession() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "DeleteSession");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::ExecuteSql() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "ExecuteSql");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::ExecuteStreamingSql() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "ExecuteStreamingSql");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::ExecuteBatchDml() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "ExecuteBatchDml");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::StreamingRead() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "StreamingRead");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BeginTransaction() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "BeginTransaction");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::Commit() {
  return std::make_shared<OperationContext>(StaticPrefix(),
                                            NextUserRequestIndex(), "Commit");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::Rollback() {
  return std::make_shared<OperationContext>(StaticPrefix(),
                                            NextUserRequestIndex(), "Rollback");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::PartitionQuery() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "PartitionQuery");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::PartitionRead() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "PartitionRead");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BatchWrite() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextUserRequestIndex(), "BatchWrite");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BackgroundCreateSession() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextBackgroundRequestIndex(), "BackgroundCreateSession");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BackgroundBatchCreateSessions() {
  return std::make_shared<OperationContext>(StaticPrefix(),
                                            NextBackgroundRequestIndex(),
                                            "BackgroundBatchCreateSessions");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BackgroundDeleteSession() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextBackgroundRequestIndex(), "BackgroundDeleteSession");
}

std::shared_ptr<OperationContext>
DefaultSpannerOperationContextFactory::BackgroundRefreshSession() {
  return std::make_shared<OperationContext>(
      StaticPrefix(), NextBackgroundRequestIndex(), "BackgroundRefreshSession");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
