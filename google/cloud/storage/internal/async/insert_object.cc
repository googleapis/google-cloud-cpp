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

#include "google/cloud/storage/internal/async/insert_object.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

future<StatusOr<google::storage::v2::Object>> InsertObject::Start() {
  auto future = result_.get_future();
  (void)rpc_->Start().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnStart(f.get());
  });
  return future;
}

void InsertObject::OnStart(bool ok) {
  if (ok) return Write();
  (void)rpc_->Finish().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) return self->OnFinish(f.get());
  });
}

void InsertObject::Write() {
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
  request_.set_finish_write(last_message);
  if (last_message) {
    auto status = Finalize(request_, wopt, *hash_function_);
    if (!status.ok()) return OnError(std::move(status));
  }
  (void)rpc_->Write(request_, std::move(wopt))
      .then([n, w = WeakFromThis()](auto f) {
        if (auto self = w.lock()) self->OnWrite(n, f.get());
      });
}

void InsertObject::OnError(Status status) {
  rpc_->Cancel();
  (void)rpc_->Finish().then(
      [w = WeakFromThis(), s = std::move(status)](auto) mutable {
        if (auto self = w.lock()) self->OnFinish(std::move(s));
      });
}

void InsertObject::OnWrite(std::size_t n, bool ok) {
  // Prepare for the next Write() request.
  request_.clear_first_message();
  request_.set_write_offset(request_.write_offset() + n);
  if (ok && !data_.empty()) return Write();
  (void)rpc_->Finish().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnFinish(f.get());
  });
}

void InsertObject::OnFinish(
    StatusOr<google::storage::v2::WriteObjectResponse> response) {
  if (!response) return result_.set_value(std::move(response).status());
  result_.set_value(std::move(*response->mutable_resource()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
