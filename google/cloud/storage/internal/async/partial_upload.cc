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

#include "google/cloud/storage/internal/async/partial_upload.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include "google/cloud/storage/internal/hash_function.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

future<StatusOr<bool>> PartialUpload::Start() {
  auto future = result_.get_future();
  Write();
  return future;
}

void PartialUpload::Write() {
  auto constexpr kMax = static_cast<std::size_t>(
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);

  request_.clear_checksummed_data();

  // Setting the contents of the message is more difficult than it should be.
  // Depending on the version of Protobuf the contents may be represented as
  // an absl::Cord or as a std::string.
  auto const n = std::min(data_.size(), kMax);
  auto next = data_.Subcord(0, n);
  data_.RemovePrefix(n);
  auto const crc32c = Crc32c(next);
  hash_function_->Update(request_.write_offset(), next, crc32c);
  auto& data = *request_.mutable_checksummed_data();
  SetContent(data, std::move(next));
  data.set_crc32c(crc32c);

  auto wopt = grpc::WriteOptions{};
  auto const last_message = data_.empty();
  if (last_message) {
    if (action_ == LastMessageAction::kFinalizeWithChecksum) {
      auto status = Finalize(request_, wopt, *hash_function_);
      if (!status.ok()) return WriteError(std::move(status));
    } else if (action_ == LastMessageAction::kFinalize) {
      request_.set_finish_write(true);
      wopt.set_last_message();
    } else if (action_ == LastMessageAction::kFlush) {
      request_.set_flush(true);
      request_.set_state_lookup(true);
    }
  }
  (void)rpc_->Write(request_, std::move(wopt))
      .then([n, w = WeakFromThis()](auto f) {
        if (auto self = w.lock()) self->OnWrite(n, f.get());
      });
}

void PartialUpload::OnWrite(std::size_t n, bool ok) {
  if (!ok) return WriteError(false);
  // Prepare for the next Write() request.
  request_.clear_first_message();
  request_.clear_flush();
  request_.clear_finish_write();
  request_.set_write_offset(request_.write_offset() + n);
  if (!data_.empty()) return Write();
  result_.set_value(true);
}

void PartialUpload::WriteError(StatusOr<bool> result) {
  if (!result.ok()) rpc_->Cancel();
  result_.set_value(std::move(result));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
