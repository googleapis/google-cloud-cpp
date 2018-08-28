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

#include "google/cloud/storage/internal/openssl_util.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string OpenSslUtils::Base64Decode(std::string const& str) {
  if (str.empty()) {
    return std::string{};
  }

  // We could compute the exact buffer size by looking at the number of padding
  // characters (=) at the end of str, but we will get the exact length later,
  // so simply compute a buffer that is big enough.
  std::string result(str.size() * 3 / 4, ' ');

  using UniqueBioChainPtr = std::unique_ptr<BIO, decltype(&BIO_free_all)>;
  using UniqueBioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

  UniqueBioPtr source(BIO_new_mem_buf(const_cast<char*>(str.data()),
                                      static_cast<int>(str.size())),
                      &BIO_free);
  if (not source) {
    std::ostringstream os;
    os << __func__ << ": cannot create BIO for source string=<" << str << ">";
    google::cloud::internal::RaiseRuntimeError(os.str());
  }
  UniqueBioPtr filter(BIO_new(BIO_f_base64()), &BIO_free);
  if (not filter) {
    std::ostringstream os;
    os << __func__ << ": cannot create BIO for Base64 decoding";
    google::cloud::internal::RaiseRuntimeError(os.str());
  }

  UniqueBioChainPtr bio(BIO_push(filter.get(), source.get()), &BIO_free_all);
  if (not bio) {
    std::ostringstream os;
    os << __func__ << ": cannot create BIO chain for Base64 decoding";
    google::cloud::internal::RaiseRuntimeError(os.str());
  }
  // Only after bio is successfully constructed we can release ownership.
  (void)source.release();
  (void)filter.release();
  BIO_set_flags(bio.get(), BIO_FLAGS_BASE64_NO_NL);

  // We do not retry, just make one call because the full stream is blocking.
  // Note that the number of bytes to read is the number of bytes we fetch from
  // the *source*, not the number of bytes that we have available in `result`.
  int len = BIO_read(bio.get(), &result[0], static_cast<int>(str.size()));
  if (len < 0) {
    std::ostringstream os;
    os << "Error parsing Base64 string [" << len << "], string=<" << str << ">";
    google::cloud::internal::RaiseRuntimeError(os.str());
  }

  result.resize(static_cast<std::size_t>(len));
  return result;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
