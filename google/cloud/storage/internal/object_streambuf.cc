// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/object_stream.h"
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
ObjectReadStreambuf::ObjectReadStreambuf(
    ReadObjectRangeRequest const& request,
    std::unique_ptr<ObjectReadSource> source)
    : source_(std::move(source)) {
  hash_validator_ = CreateHashValidator(request);
}

ObjectReadStreambuf::ObjectReadStreambuf(ReadObjectRangeRequest const& request,
                                         Status status)
    : source_(new ObjectReadErrorSource(status)) {
  // TODO(coryan) - revisit this, we probably do not need the validator.
  hash_validator_ = CreateHashValidator(request);
  status_ = std::move(status);
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

  current_ios_buffer_.resize(128 * 1024);
  std::size_t n = current_ios_buffer_.size();
  StatusOr<ReadSourceResult> read_result =
      source_->Read(current_ios_buffer_.data(), n);
  if (!read_result.ok()) {
    return std::move(read_result).status();
  }
  // assert(n <= current_ios_buffer_.size())
  current_ios_buffer_.resize(read_result->bytes_received);

  for (auto const& kv : read_result->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  if (read_result->response.status_code >= 300) {
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
  if (!next_char) {
    return ReportError(next_char.status());
  }

  if (*next_char == traits_type::eof()) {
    hash_validator_result_ = std::move(*hash_validator_).Finish();
    if (hash_validator_result_.is_mismatch) {
      std::string msg;
      msg += __func__;
      msg += "(): mismatched hashes in download";
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      throw HashMismatchError(msg, hash_validator_result_.received,
                              hash_validator_result_.computed);
#else
      msg += ", expected=";
      msg += hash_validator_result_.computed;
      msg += ", received=";
      msg += hash_validator_result_.received;
      status_ = Status(StatusCode::kDataLoss, std::move(msg));
      return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    }
  }

  return *next_char;
}

std::streamsize ObjectReadStreambuf::xsgetn(char* s, std::streamsize count) {
  GCP_LOG(INFO) << __func__ << "(): count=" << count
                << ", in_avail=" << in_avail() << ", status=" << status_;
  // This function optimizes stream.read(), the data is copied directly from the
  // data source (typically libcurl) into a buffer provided by the application.
  std::streamsize offset = 0;
  if (!status_.ok()) {
    return 0;
  }

  // Maybe the internal get area is enough to satisfy this request, no need to
  // read more in that case:
  auto from_internal = (std::min)(count, in_avail());
  std::memcpy(s, gptr(), static_cast<std::size_t>(from_internal));
  gbump(static_cast<int>(from_internal));
  offset += from_internal;
  if (offset >= count) {
    GCP_LOG(INFO) << __func__ << "(): count=" << count
                  << ", in_avail=" << in_avail() << ", offset=" << offset;
    return offset;
  }

  StatusOr<ReadSourceResult> read_result =
      source_->Read(s + offset, static_cast<std::size_t>(count - offset));
  GCP_LOG(INFO) << __func__ << "(): count=" << count
                << ", in_avail=" << in_avail() << ", offset=" << offset
                << ", status=" << read_result.status();
  // If there was an error set the internal state, but we still return the
  // number of bytes.
  if (!read_result) {
    status_ = std::move(read_result).status();
    return offset;
  }

  hash_validator_->Update(s + offset, read_result->bytes_received);
  offset += read_result->bytes_received;

  for (auto const& kv : read_result->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  if (read_result->response.status_code >= 300) {
    status_ = AsStatus(read_result->response);
  }

  return offset;
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

StatusOr<HttpResponse> ObjectWriteStreambuf::Close() {
  pubsync();
  return DoClose();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
