// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_MOCKS_MOCK_ASYNC_WRITER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_MOCKS_MOCK_ASYNC_WRITER_CONNECTION_H

#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockAsyncWriterConnection
    : public storage_experimental::AsyncWriterConnection {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(std::string, UploadId, (), (const, override));
  MOCK_METHOD((absl::variant<std::int64_t, google::storage::v2::Object>),
              PersistedState, (), (const, override));
  MOCK_METHOD(future<Status>, Write, (storage_experimental::WritePayload),
              (override));
  MOCK_METHOD(future<StatusOr<google::storage::v2::Object>>, Finalize,
              (storage_experimental::WritePayload), (override));
  MOCK_METHOD(future<Status>, Flush, (storage_experimental::WritePayload),
              (override));
  MOCK_METHOD(future<StatusOr<std::int64_t>>, Query, (), (override));
  MOCK_METHOD(RpcMetadata, GetRequestMetadata, (), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_MOCKS_MOCK_ASYNC_WRITER_CONNECTION_H
