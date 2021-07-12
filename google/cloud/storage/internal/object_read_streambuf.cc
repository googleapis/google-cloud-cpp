// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/hash_mismatch_error.h"
#include "absl/memory/memory.h"
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

ObjectReadStreambuf::ObjectReadStreambuf(
    ReadObjectRangeRequest const& request,
    std::unique_ptr<ObjectReadSource> source, std::streamoff pos_in_stream)
    : source_(std::move(source)),
      source_pos_(pos_in_stream),
      hash_validator_(CreateHashValidator(request)) {}

ObjectReadStreambuf::ObjectReadStreambuf(ReadObjectRangeRequest const&,
                                         Status status)
    : source_(new ObjectReadErrorSource(status)),
      source_pos_(-1),
      hash_validator_(absl::make_unique<NullHashValidator>()),
      status_(std::move(status)) {}

ObjectReadStreambuf::pos_type ObjectReadStreambuf::seekpos(
    pos_type /*pos*/, std::ios_base::openmode /*which*/) {
  // TODO(#4013): Implement proper seeking.
  return -1;
}

ObjectReadStreambuf::pos_type ObjectReadStreambuf::seekoff(
    off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) {
  // TODO(#4013): Implement proper seeking.
  // Seeking is non-trivial because the hash validator and `source_` have to be
  // recreated in the general case, which doesn't fit the current code
  // organization.  We can, however, at least implement the bare minimum of this
  // function allowing `tellg()` to work.
  if (which == std::ios_base::in && dir == std::ios_base::cur && off == 0) {
    return source_pos_ - in_avail();
  }
  return -1;
}

bool ObjectReadStreambuf::IsOpen() const { return source_->IsOpen(); }

void ObjectReadStreambuf::Close() {
  auto response = source_->Close();
  if (!response.ok()) {
    ReportError(std::move(response).status());
  }
}

StatusOr<ObjectReadStreambuf::int_type> ObjectReadStreambuf::Peek() {
  if (!IsOpen()) {
    // The stream is closed, reading from a closed stream can happen if there is
    // no object to read from, or the object is empty. In that case just setup
    // an empty (but valid) region and verify the checksums.
    SetEmptyRegion();
    return traits_type::eof();
  }

  auto constexpr kInitialPeekRead = 128 * 1024;
  current_ios_buffer_.resize(kInitialPeekRead);
  std::size_t n = current_ios_buffer_.size();
  StatusOr<ReadSourceResult> read_result =
      source_->Read(current_ios_buffer_.data(), n);
  if (!read_result.ok()) {
    return std::move(read_result).status();
  }
  source_pos_ += read_result->bytes_received;
  // assert(n <= current_ios_buffer_.size())
  current_ios_buffer_.resize(read_result->bytes_received);

  auto hint = headers_.end();
  for (auto& kv : read_result->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    hint =
        std::next(headers_.emplace_hint(hint, kv.first, std::move(kv.second)));
  }
  if (read_result->response.status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(read_result->response);
  }

  if (!current_ios_buffer_.empty()) {
    char* data = current_ios_buffer_.data();
    hash_validator_->Update(data, current_ios_buffer_.size());
    setg(data, data, data + current_ios_buffer_.size());
    return traits_type::to_int_type(*data);
  }

  // This is an actual EOF, there is no more data to download, create an
  // empty (but valid) region:
  SetEmptyRegion();
  return traits_type::eof();
}

ObjectReadStreambuf::int_type ObjectReadStreambuf::underflow() {
  auto next_char = Peek();
  if (!next_char) return ReportError(next_char.status());
  if (*next_char != traits_type::eof()) return *next_char;

  auto msg = FinishValidator(__func__);
  if (!hash_validator_result_.is_mismatch) return traits_type::eof();
  if (status_.ok()) {
    // If there is an existing error, we should report that instead because
    // it is more specific. For example, every permanent network error will
    // produce invalid checksums, but that is not the interesting
    // information.
    status_ = Status(StatusCode::kDataLoss, msg);
  }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  throw HashMismatchError(msg, received_hash(), computed_hash());
#else
  return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

std::streamsize ObjectReadStreambuf::xsgetn(char* s, std::streamsize count) {
  // This function optimizes stream.read(), the data is copied directly from the
  // data source (typically libcurl) into a buffer provided by the application.
  std::streamsize offset = 0;
  if (!status_.ok()) {
    return 0;
  }

  auto const* function_name = __func__;
  auto run_validator_if_closed = [this, function_name, &offset](Status s) {
    ReportError(std::move(s));
    // Only validate the checksums once the stream is closed.
    if (IsOpen()) return offset;
    auto msg = FinishValidator(function_name);
    if (!hash_validator_result_.is_mismatch) return offset;
      // The only way to report errors from a std::basic_streambuf<> (which this
      // class derives from) is to throw exceptions:
      //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
      // but we need to be able to report errors when the application has
      // disabled exceptions via `-fno-exceptions` or a similar option. In that
      // case we set `status_`, and report the error as an 0-byte read. This is
      // obviously not ideal, but it is the best we can do when the application
      // disables the standard mechanism to signal errors.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    throw HashMismatchError(msg, received_hash(), computed_hash());
#else
    return std::streamsize(0);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  };

  // Maybe the internal get area is enough to satisfy this request, no need to
  // read more in that case:
  auto from_internal = (std::min)(count, in_avail());
  if (from_internal > 0) {
    std::memcpy(s, gptr(), static_cast<std::size_t>(from_internal));
  }
  gbump(static_cast<int>(from_internal));
  offset += from_internal;
  if (offset >= count) {
    return run_validator_if_closed(Status());
  }

  StatusOr<ReadSourceResult> read_result =
      source_->Read(s + offset, static_cast<std::size_t>(count - offset));
  // If there was an error set the internal state, but we still return the
  // number of bytes.
  if (!read_result) {
    return run_validator_if_closed(std::move(read_result).status());
  }

  hash_validator_->Update(s + offset, read_result->bytes_received);
  offset += read_result->bytes_received;
  source_pos_ += read_result->bytes_received;

  for (auto const& kv : read_result->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  if (read_result->response.status_code >= HttpStatusCode::kMinNotSuccess) {
    return run_validator_if_closed(AsStatus(read_result->response));
  }
  return run_validator_if_closed(Status());
}

ObjectReadStreambuf::int_type ObjectReadStreambuf::ReportError(Status status) {
  // The only way to report errors from a std::basic_streambuf<> (which this
  // class derives from) is to throw exceptions:
  //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
  // but we need to be able to report errors when the application has disabled
  // exceptions via `-fno-exceptions` or a similar option. In that case we set
  // `status_`, and report the error as an EOF. This is obviously not ideal,
  // but it is the best we can do when the application disables the standard
  // mechanism to signal errors.
  if (status.ok()) {
    return traits_type::eof();
  }
  status_ = std::move(status);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  google::cloud::internal::ThrowStatus(status_);
#else
  return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

void ObjectReadStreambuf::SetEmptyRegion() {
  current_ios_buffer_.clear();
  current_ios_buffer_.push_back('\0');
  char* data = &current_ios_buffer_[0];
  setg(data, data + 1, data + 1);
}

std::string ObjectReadStreambuf::FinishValidator(char const* function_name) {
  hash_validator_result_ = std::move(*hash_validator_).Finish();
  if (!hash_validator_result_.is_mismatch) return {};

  std::string msg;
  msg += function_name;
  msg += "(): mismatched hashes in download";
  msg += ", computed=";
  msg += computed_hash();
  msg += ", received=";
  msg += received_hash();
  if (status_.ok()) {
    // If there is an existing error, we should report that instead because
    // it is more specific, for example, every permanent network error will
    // produce invalid checksums, but that is not the interesting information.
    status_ = Status(StatusCode::kDataLoss, msg);
  }
  return msg;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
