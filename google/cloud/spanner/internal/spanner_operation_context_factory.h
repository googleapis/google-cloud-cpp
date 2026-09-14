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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_OPERATION_CONTEXT_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_OPERATION_CONTEXT_FACTORY_H

#include "google/cloud/spanner/internal/operation_context.h"
#include "google/cloud/spanner/version.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class SpannerOperationContextFactory {
 public:
  virtual ~SpannerOperationContextFactory() = default;

  // User-facing RPC context factories (use next_user_request_index_)
  virtual OperationContext CreateSession() = 0;
  virtual OperationContext BatchCreateSessions() = 0;
  virtual OperationContext GetSession() = 0;
  virtual OperationContext ListSessions() = 0;
  virtual OperationContext DeleteSession() = 0;
  virtual OperationContext ExecuteSql() = 0;
  virtual OperationContext ExecuteStreamingSql() = 0;
  virtual OperationContext ExecuteBatchDml() = 0;
  virtual OperationContext StreamingRead() = 0;
  virtual OperationContext BeginTransaction() = 0;
  virtual OperationContext Commit() = 0;
  virtual OperationContext Rollback() = 0;
  virtual OperationContext PartitionQuery() = 0;
  virtual OperationContext PartitionRead() = 0;
  virtual OperationContext BatchWrite() = 0;

  // Background maintenance context factories (use
  // next_background_request_index_)
  virtual OperationContext BackgroundCreateSession() = 0;
  virtual OperationContext BackgroundBatchCreateSessions() = 0;
  virtual OperationContext BackgroundDeleteSession() = 0;
  virtual OperationContext BackgroundRefreshSession() = 0;
};

class DefaultSpannerOperationContextFactory
    : public SpannerOperationContextFactory {
 public:
  // Explicit constructor without default arguments per internal guidelines.
  DefaultSpannerOperationContextFactory(
      std::uint64_t client_id,
      std::shared_ptr<std::string const> process_random_id);

  OperationContext CreateSession() override;
  OperationContext BatchCreateSessions() override;
  OperationContext GetSession() override;
  OperationContext ListSessions() override;
  OperationContext DeleteSession() override;
  OperationContext ExecuteSql() override;
  OperationContext ExecuteStreamingSql() override;
  OperationContext ExecuteBatchDml() override;
  OperationContext StreamingRead() override;
  OperationContext BeginTransaction() override;
  OperationContext Commit() override;
  OperationContext Rollback() override;
  OperationContext PartitionQuery() override;
  OperationContext PartitionRead() override;
  OperationContext BatchWrite() override;

  OperationContext BackgroundCreateSession() override;
  OperationContext BackgroundBatchCreateSessions() override;
  OperationContext BackgroundDeleteSession() override;
  OperationContext BackgroundRefreshSession() override;

 private:
  std::shared_ptr<std::string const> StaticPrefix();
  std::uint64_t NextUserRequestIndex();
  std::uint64_t NextBackgroundRequestIndex();

  std::uint64_t client_id_;
  std::shared_ptr<std::string const> process_random_id_;
  std::shared_ptr<std::string const> static_prefix_;
#ifndef _WIN32
  pid_t cached_pid_;
#endif
  std::mutex mu_;
  std::atomic<std::uint64_t> next_user_request_index_{1};
  std::atomic<std::uint64_t> next_background_request_index_{1};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_OPERATION_CONTEXT_FACTORY_H
