// Copyright 2021 Google LLC
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

#include "google/cloud/storage/object_write_stream.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
std::unique_ptr<internal::ObjectWriteStreambuf> MakeErrorStreambuf() {
  return absl::make_unique<internal::ObjectWriteStreambuf>(
      absl::make_unique<internal::ResumableUploadSessionError>(
          Status(StatusCode::kUnimplemented, "null stream")),
      Status(StatusCode::kUnimplemented, "null stream"),
      /*max_buffer_size=*/0, internal::CreateNullHashFunction(),
      internal::HashValues{}, internal::CreateNullHashValidator(),
      AutoFinalizeConfig::kDisabled);
}
}  // namespace
static_assert(std::is_move_assignable<ObjectWriteStream>::value,
              "storage::ObjectWriteStream must be move assignable.");
static_assert(std::is_move_constructible<ObjectWriteStream>::value,
              "storage::ObjectWriteStream must be move constructible.");

ObjectWriteStream::ObjectWriteStream()
    : ObjectWriteStream(MakeErrorStreambuf()) {}

ObjectWriteStream::ObjectWriteStream(
    std::unique_ptr<internal::ObjectWriteStreambuf> buf)
    : std::basic_ostream<char>(buf.get()), buf_(std::move(buf)) {
  // If buf_ is already closed, update internal state to represent
  // the fact that no more bytes can be uploaded to this object.
  if (buf_ && !buf_->IsOpen()) CloseBuf();
}

ObjectWriteStream::ObjectWriteStream(ObjectWriteStream&& rhs) noexcept
    : std::basic_ostream<char>(std::move(rhs)),
      // The spec guarantees the base class move constructor only changes a few
      // member variables in `std::basic_istream<>`, and there is no spooky
      // action through virtual functions because there are no virtual
      // functions.  A good summary of the specification is "it calls the
      // default constructor and then calls std::basic_ios<>::move":
      //   https://en.cppreference.com/w/cpp/io/basic_ios/move
      // In fact, as that page indicates, the base classes are designed such
      // that derived classes can define their own move constructor and move
      // assignment.
      buf_(std::move(rhs.buf_)),
      metadata_(std::move(rhs.metadata_)),
      headers_(std::move(rhs.headers_)),
      payload_(std::move(rhs.payload_)) {
  rhs.buf_ = MakeErrorStreambuf();
  rhs.set_rdbuf(rhs.buf_.get());
  set_rdbuf(buf_.get());
  if (!buf_) {
    setstate(std::ios::badbit | std::ios::eofbit);
  } else {
    if (!buf_->last_status().ok()) setstate(std::ios::badbit);
    if (!buf_->IsOpen()) setstate(std::ios::eofbit);
  }
}

ObjectWriteStream::~ObjectWriteStream() {
  if (!IsOpen()) return;
  // Disable exceptions, even if the application had enabled exceptions the
  // destructor is supposed to mask them.
  exceptions(std::ios_base::goodbit);
  buf_->AutoFlushFinal();
}

void ObjectWriteStream::Close() {
  if (!buf_) return;
  CloseBuf();
}

void ObjectWriteStream::CloseBuf() {
  auto response = buf_->Close();
  if (!response.ok()) {
    metadata_ = std::move(response).status();
    setstate(std::ios_base::badbit);
    return;
  }
  headers_ = {};
  if (response->payload.has_value()) {
    metadata_ = *std::move(response->payload);
  }
  if (metadata_ && !buf_->ValidateHash(*metadata_)) {
    setstate(std::ios_base::badbit);
  }
}

void ObjectWriteStream::Suspend() && {
  ObjectWriteStream tmp;
  swap(tmp);
  tmp.buf_.reset();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
