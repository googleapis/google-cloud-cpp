// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_STREAM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_STREAM_H

#include "google/cloud/storage/internal/async/open_object.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/storage/v2/storage.pb.h>
#include <atomic>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An open bidi streaming read RPC.
 *
 * gRPC imposes a number of restrictions on how to close bidi streaming RPCs.
 * This class automates most of these restrictions. In particular, it waits
 * (using background threads) until all pending `Read()` and `Write()` calls
 * complete before trying to `Finish()` and then delete the stream.
 *
 * gRPC will assert if one deletes a streaming read-write RPC before waiting for
 * the result of `Finish()` calls. It will also assert if one calls `Finish()`
 * while there are pending `Read()` or `Write()` calls.
 *
 * This class tracks what operations, if any, are pending. On cancel, it waits
 * until all pending operations complete, then calls `Finish()` and then deletes
 * the streaming RPC (and itself).
 *
 * One may wonder if this class should be a reusable feature in the common
 * libraries. That may be, but there are not enough use-cases to determine how
 * a generalized version would look like. And in other cases (e.g.
 * `AsyncWriterConnectionImpl`) the usage was restricted in ways that did not
 * require this helper class.
 */
class OpenStream : public std::enable_shared_from_this<OpenStream> {
 public:
  using ReadType = absl::optional<google::storage::v2::BidiReadObjectResponse>;

  explicit OpenStream(std::shared_ptr<OpenObject::StreamingRpc> rpc);

  void Cancel();
  future<bool> Write(google::storage::v2::BidiReadObjectRequest const& request);
  future<ReadType> Read();
  future<Status> Finish();

 private:
  void OnWrite();
  void OnRead();
  void MaybeFinish();

  std::atomic<bool> cancel_{false};
  std::atomic<bool> pending_read_{false};
  std::atomic<bool> pending_write_{false};
  std::atomic<bool> finish_issued_{false};
  std::shared_ptr<OpenObject::StreamingRpc> rpc_;
};

using OpenStreamFactory =
    std::function<future<StatusOr<std::shared_ptr<OpenStream>>>(
        google::storage::v2::BidiReadObjectRequest)>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_STREAM_H
