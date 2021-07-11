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
#include "google/cloud/storage/internal/hash_function.h"
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
      hash_function_(CreateHashFunction(request)),
      hash_validator_(CreateHashValidator(request)) {}

ObjectReadStreambuf::ObjectReadStreambuf(ReadObjectRangeRequest const&,
                                         Status status)
    : source_(new ObjectReadErrorSource(status)),
      source_pos_(-1),
      hash_validator_(CreateNullHashValidator()),
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

void ObjectReadStreambuf::ThrowHashMismatchDelegate(const char* function_name) {
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
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // The only way to report errors from a std::basic_streambuf<> (which this
  // class derives from) is to throw exceptions:
  //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
  // but we need to be able to report errors when the application has
  // disabled exceptions via `-fno-exceptions` or a similar option. In that
  // case we set `status_`, and report the error as an 0-byte read. This is
  // obviously not ideal, but it is the best we can do when the application
  // disables the standard mechanism to signal errors.
  throw HashMismatchError(msg, received_hash(), computed_hash());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

bool ObjectReadStreambuf::ValidateHashes(char const* function_name) {
  // This function is called once the stream is "closed" (either an explicit
  // `Close()` call or a permanent error). After this point the validator is
  // not usable.
  auto function = std::move(hash_function_);
  auto validator = std::move(hash_validator_);
  hash_validator_result_ =
      std::move(*validator).Finish(std::move(*function).Finish());
  computed_hash_ = FormatComputedHashes(hash_validator_result_);
  received_hash_ = FormatReceivedHashes(hash_validator_result_);
  if (!hash_validator_result_.is_mismatch) return true;
  ThrowHashMismatchDelegate(function_name);
  return false;
}

bool ObjectReadStreambuf::CheckPreconditions(char const* function_name) {
  if (hash_validator_result_.is_mismatch) {
    ThrowHashMismatchDelegate(function_name);
  }
  if (in_avail() != 0) return true;
  return status_.ok() && IsOpen();
}

ObjectReadStreambuf::int_type ObjectReadStreambuf::underflow() {
  if (!CheckPreconditions(__func__)) return traits_type::eof();

  // If this function is called, then the internal buffer must be empty. We will
  // perform a read into a new buffer and reset the input area to use this
  // buffer.
  auto constexpr kInitialPeekRead = 128 * 1024;
  std::vector<char> buffer(kInitialPeekRead);
  auto const offset = xsgetn(buffer.data(), kInitialPeekRead);
  if (offset == 0) return traits_type::eof();

  buffer.resize(static_cast<std::size_t>(offset));
  buffer.swap(current_ios_buffer_);
  char* data = current_ios_buffer_.data();
  setg(data, data, data + current_ios_buffer_.size());
  return traits_type::to_int_type(*data);
}

std::streamsize ObjectReadStreambuf::xsgetn(char* s, std::streamsize count) {
  if (!CheckPreconditions(__func__)) return 0;

  // This function optimizes stream.read(), the data is copied directly from the
  // data source (typically libcurl) into a buffer provided by the application.
  std::streamsize offset = 0;

  // Maybe the internal get area is enough to satisfy this request, no need to
  // read more in that case:
  auto from_internal = (std::min)(count, in_avail());
  if (from_internal > 0) {
    std::memcpy(s, gptr(), static_cast<std::size_t>(from_internal));
  }
  gbump(static_cast<int>(from_internal));
  offset += from_internal;
  // If we got all the data requested, there is no need for additional reads.
  // Likewise, if the underlying transport is closed, whatever we got is all the
  // data available.
  if (offset >= count || !IsOpen()) return offset;

  auto const* function_name = __func__;
  auto run_validator_if_closed = [this, function_name, &offset](Status s) {
    ReportError(std::move(s));
    // Only validate the checksums once the stream is closed.
    if (IsOpen()) return offset;
    return ValidateHashes(function_name) ? offset : 0;
  };

  StatusOr<ReadSourceResult> read =
      source_->Read(s + offset, static_cast<std::size_t>(count - offset));
  // If there was an error set the internal state, but we still return the
  // number of bytes.
  if (!read) return run_validator_if_closed(std::move(read).status());

  hash_function_->Update(s + offset, read->bytes_received);
  offset += static_cast<std::streamsize>(read->bytes_received);
  source_pos_ += static_cast<std::streamoff>(read->bytes_received);

  for (auto const& kv : read->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  return run_validator_if_closed(Status());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
