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
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/base64.h>
#endif  // OPENSSL_IS_BORINGSSL

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string OpenSslUtils::Base64Decode(std::string const& str) {
#ifdef OPENSSL_IS_BORINGSSL
  std::size_t decoded_size;
  EVP_DecodedLength(&decoded_size, str.size());
  std::string result(decoded_size, '\0');
  EVP_DecodeBase64(
      reinterpret_cast<std::uint8_t*>(&result[0]), &decoded_size, result.size(),
      reinterpret_cast<std::uint8_t const*>(str.data()), str.size());
  result.resize(decoded_size);
  return result;
#else
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
    google::cloud::internal::ThrowRuntimeError(os.str());
  }
  UniqueBioPtr filter(BIO_new(BIO_f_base64()), &BIO_free);
  if (not filter) {
    std::ostringstream os;
    os << __func__ << ": cannot create BIO for Base64 decoding";
    google::cloud::internal::ThrowRuntimeError(os.str());
  }

  // Based on the documentation this never fails, so we can transfer ownership
  // of `filter` and `source` and do not need to check the result.
  UniqueBioChainPtr bio(BIO_push(filter.release(), source.release()),
                        &BIO_free_all);
  BIO_set_flags(bio.get(), BIO_FLAGS_BASE64_NO_NL);

  // We do not retry, just make one call because the full stream is blocking.
  // Note that the number of bytes to read is the number of bytes we fetch from
  // the *source*, not the number of bytes that we have available in `result`.
  int len = BIO_read(bio.get(), &result[0], static_cast<int>(str.size()));
  if (len < 0) {
    std::ostringstream os;
    os << "Error parsing Base64 string [" << len << "], string=<" << str << ">";
    google::cloud::internal::ThrowRuntimeError(os.str());
  }

  result.resize(static_cast<std::size_t>(len));
  return result;
#endif  // OPENSSL_IS_BORINGSSL
}

std::string OpenSslUtils::Base64Encode(std::string const& str) {
#ifdef OPENSSL_IS_BORINGSSL
  std::size_t encoded_size;
  EVP_EncodedLength(&encoded_size, str.size());
  std::string result(encoded_size, '\0');
  std::size_t out_size = EVP_EncodeBlock(
      reinterpret_cast<std::uint8_t*>(&result[0]),
      reinterpret_cast<std::uint8_t const*>(str.data()), str.size());
  result.resize(out_size);
  return result;
#else
  auto bio_chain = MakeBioChainForBase64Transcoding();
  int retval = 0;

  while (true) {
    retval = BIO_write(static_cast<BIO*>(bio_chain.get()), str.c_str(),
                       static_cast<int>(str.length()));
    if (retval > 0) {
      break;  // Positive value == successful write.
    }
    if (not BIO_should_retry(static_cast<BIO*>(bio_chain.get()))) {
      std::ostringstream err_builder;
      err_builder << "Permanent error in " << __func__ << ": "
                  << "BIO_write returned non-retryable value of " << retval;
      google::cloud::internal::ThrowRuntimeError(err_builder.str());
    }
  }
  // Tell the b64 encoder that we're done writing data, thus prompting it to
  // add trailing '=' characters for padding if needed.
  while (true) {
    retval = BIO_flush(static_cast<BIO*>(bio_chain.get()));
    if (retval > 0) {
      break;  // Positive value == successful flush.
    }
    if (not BIO_should_retry(static_cast<BIO*>(bio_chain.get()))) {
      std::ostringstream err_builder;
      err_builder << "Permanent error in " << __func__ << ": "
                  << "BIO_flush returned non-retryable value of " << retval;
      google::cloud::internal::ThrowRuntimeError(err_builder.str());
    }
  }

  // This buffer belongs to the BIO chain and is freed upon its destruction.
  BUF_MEM* buf_mem;
  BIO_get_mem_ptr(static_cast<BIO*>(bio_chain.get()), &buf_mem);
  // Return a string copy of the buffer's bytes, as the buffer will be freed
  // upon this method's exit.
  return std::string(buf_mem->data, buf_mem->length);
#endif  // OPENSSL_IS_BORINGSSL
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
