// Copyright 2019 Google LLC
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
#include "google/cloud/internal/throw_delegate.h"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/base64.h>
#endif  // OPENSSL_IS_BORINGSSL
#include <memory>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

namespace {
// The name of the function to free an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
inline std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)>
GetDigestCtx() {
  return std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)>(
      EVP_MD_CTX_create(), &EVP_MD_CTX_destroy);
};
#else
inline std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> GetDigestCtx() {
  return std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
      EVP_MD_CTX_new(), &EVP_MD_CTX_free);
};
#endif

#ifndef OPENSSL_IS_BORINGSSL
/**
 * Build a BIO chain for Base 64 encoding and decoding.
 *
 * BIO chains are a OpenSSL abstraction to perform I/O (including from memory
 * buffers) with transformations. This function takes a BIO object and builds a
 * chain that:
 *
 * - For writes, it performs Base 64 encoding and then writes the encoded data
 *   into @p mem_io.
 * - For reads, it extracts data from @p mem_io and then decodes it using
 *   Base64.
 *
 */
std::unique_ptr<BIO, decltype(&BIO_free_all)> PushBase64Transcoding(
    std::unique_ptr<BIO, decltype(&BIO_free)> mem_io) {
  auto base64_io = std::unique_ptr<BIO, decltype(&BIO_free)>(
      BIO_new(BIO_f_base64()), &BIO_free);
  if (!(base64_io && mem_io)) {
    std::ostringstream err_builder;
    err_builder << "Permanent error in " << __func__ << ": "
                << "Could not allocate BIO* for Base64 encoding.";
    google::cloud::internal::ThrowRuntimeError(err_builder.str());
  }
  auto bio_chain = std::unique_ptr<BIO, decltype(&BIO_free_all)>(
      // Output from a b64 encoder should go to an in-memory sink.
      BIO_push(base64_io.release(), mem_io.release()),
      // Make sure we free all resources in this chain upon destruction.
      &BIO_free_all);
  // Don't use newlines as a signal for when to flush buffers.
  BIO_set_flags(bio_chain.get(), BIO_FLAGS_BASE64_NO_NL);
  return bio_chain;
}
#endif  // OPENSSL_IS_BORINGSSL

std::string Base64Encode(std::uint8_t const* bytes, std::size_t bytes_size) {
#ifdef OPENSSL_IS_BORINGSSL
  std::size_t encoded_size;
  EVP_EncodedLength(&encoded_size, bytes_size);
  std::vector<std::uint8_t> result(encoded_size);
  std::size_t out_size = EVP_EncodeBlock(result.data(), bytes, bytes_size);
  result.resize(out_size);
  return {result.begin(), result.end()};
#else
  auto mem_io = std::unique_ptr<BIO, decltype(&BIO_free)>(BIO_new(BIO_s_mem()),
                                                          &BIO_free);
  auto bio_chain = PushBase64Transcoding(std::move(mem_io));

  // These BIO_*() operations are guaranteed not to block, consult the NOTES in:
  //   https://www.openssl.org/docs/man1.1.1/man3/BIO_s_mem.html
  // for details.
  int retval = BIO_write(bio_chain.get(), bytes, static_cast<int>(bytes_size));
  if (retval <= 0) {
    std::ostringstream err_builder;
    err_builder << "Permanent error in " << __func__ << ": "
                << "BIO_write returned non-retryable value of " << retval;
    google::cloud::internal::ThrowRuntimeError(err_builder.str());
  }

  // Tell the b64 encoder that we're done writing data, thus prompting it to
  // add trailing '=' characters for padding if needed.
  retval = BIO_flush(bio_chain.get());
  if (retval <= 0) {
    std::ostringstream err_builder;
    err_builder << "Permanent error in " << __func__ << ": "
                << "BIO_flush returned non-retryable value of " << retval;
    google::cloud::internal::ThrowRuntimeError(err_builder.str());
  }

  // This buffer belongs to the BIO chain and is freed upon its destruction.
  BUF_MEM* buf_mem;
  BIO_get_mem_ptr(bio_chain.get(), &buf_mem);
  // Return a string copy of the buffer's bytes, as the buffer will be freed
  // upon this method's exit.
  return std::string(buf_mem->data, buf_mem->length);
#endif  // OPENSSL_IS_BORINGSSL
}
}  // namespace

std::vector<std::uint8_t> Base64Decode(std::string const& str) {
#ifdef OPENSSL_IS_BORINGSSL
  std::size_t decoded_size;
  EVP_DecodedLength(&decoded_size, str.size());
  std::vector<std::uint8_t> result(decoded_size);
  EVP_DecodeBase64(result.data(), &decoded_size, result.size(),
                   reinterpret_cast<unsigned char const*>(str.data()),
                   str.size());
  result.resize(decoded_size);
  return result;
#else
  if (str.empty()) {
    return {};
  }

  std::unique_ptr<BIO, decltype(&BIO_free)> source(
      BIO_new_mem_buf(str.data(), static_cast<int>(str.size())), &BIO_free);
  auto bio = PushBase64Transcoding(std::move(source));

  // We could compute the exact buffer size by looking at the number of padding
  // characters (=) at the end of str, but we will get the exact length later,
  // so simply compute a buffer that is big enough.
  std::vector<std::uint8_t> result(str.size() * 3 / 4);

  // We do not retry, just make one call because the full stream is blocking.
  // Note that the number of bytes to read is the number of bytes we fetch from
  // the *source*, not the number of bytes that we have available in `result`.
  int len = BIO_read(bio.get(), result.data(), static_cast<int>(str.size()));
  if (len < 0) {
    std::ostringstream os;
    os << "Error parsing Base64 string [" << len << "], string=<" << str << ">";
    google::cloud::internal::ThrowRuntimeError(os.str());
  }

  result.resize(static_cast<std::size_t>(len));
  return result;
#endif  // OPENSSL_IS_BORINGSSL
}

std::string Base64Encode(std::string const& str) {
  return Base64Encode(reinterpret_cast<unsigned char const*>(str.data()),
                      str.size());
}

std::string Base64Encode(std::vector<std::uint8_t> const& bytes) {
  return Base64Encode(bytes.data(), bytes.size());
}

std::vector<std::uint8_t> SignStringWithPem(
    std::string const& str, std::string const& pem_contents,
    storage::oauth2::JwtSigningAlgorithms alg) {
  using ::google::cloud::storage::oauth2::JwtSigningAlgorithms;

  // We check for failures several times, so we shorten this into a lambda
  // to avoid bloating the code with alloc/init checks.
  const char* func_name = __func__;  // Avoid using the lambda name instead.
  auto handle_openssl_failure = [&func_name](const char* error_msg) -> void {
    std::ostringstream err_builder;
    err_builder << "Permanent error in " << func_name
                << " (failed to sign string with PEM key):\n"
                << error_msg;
    google::cloud::internal::ThrowRuntimeError(err_builder.str());
  };

  auto digest_ctx = GetDigestCtx();
  if (!digest_ctx) {
    handle_openssl_failure("Could not create context for OpenSSL digest.");
  }

  EVP_MD const* digest_type = nullptr;
  switch (alg) {
    case JwtSigningAlgorithms::RS256:
      digest_type = EVP_sha256();
      break;
  }
  if (digest_type == nullptr) {
    handle_openssl_failure("Could not find specified digest in OpenSSL.");
  }

  auto pem_buffer = std::unique_ptr<BIO, decltype(&BIO_free)>(
      BIO_new_mem_buf(pem_contents.data(),
                      static_cast<int>(pem_contents.length())),
      &BIO_free);
  if (!pem_buffer) {
    handle_openssl_failure("Could not create PEM buffer.");
  }

  auto private_key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(
      PEM_read_bio_PrivateKey(
          pem_buffer.get(),
          nullptr,  // EVP_PKEY **x
          nullptr,  // pem_password_cb *cb -- a custom callback.
          // void *u -- this represents the password for the PEM (only
          // applicable for formats such as PKCS12 (.p12 files) that use
          // a password, which we don't currently support.
          nullptr),
      &EVP_PKEY_free);
  if (!private_key) {
    handle_openssl_failure("Could not parse PEM to get private key.");
  }

  int const digest_sign_success_code = 1;
  if (digest_sign_success_code !=
      EVP_DigestSignInit(digest_ctx.get(),
                         nullptr,  // `EVP_PKEY_CTX **pctx`
                         digest_type,
                         nullptr,  // `ENGINE *e`
                         private_key.get())) {
    handle_openssl_failure("Could not initialize PEM digest.");
  }

  if (digest_sign_success_code !=
      EVP_DigestSignUpdate(digest_ctx.get(), str.data(), str.length())) {
    handle_openssl_failure("Could not update PEM digest.");
  }

  std::size_t signed_str_size = 0;
  // Calling this method with a nullptr buffer will populate our size var
  // with the resulting buffer's size. This allows us to then call it again,
  // with the correct buffer and size, which actually populates the buffer.
  if (digest_sign_success_code !=
      EVP_DigestSignFinal(digest_ctx.get(),
                          nullptr,  // unsigned char *sig
                          &signed_str_size)) {
    handle_openssl_failure("Could not finalize PEM digest (1/2).");
  }

  std::vector<unsigned char> signed_str(signed_str_size);
  if (digest_sign_success_code != EVP_DigestSignFinal(digest_ctx.get(),
                                                      signed_str.data(),
                                                      &signed_str_size)) {
    handle_openssl_failure("Could not finalize PEM digest (2/2).");
  }

  return {signed_str.begin(), signed_str.end()};
}

std::vector<std::uint8_t> UrlsafeBase64Decode(std::string const& str) {
  if (str.empty()) {
    return {};
  }
  std::string b64str = str;
  std::replace(b64str.begin(), b64str.end(), '-', '+');
  std::replace(b64str.begin(), b64str.end(), '_', '/');
  // To restore the padding there are only two cases:
  //    https://en.wikipedia.org/wiki/Base64#Decoding_Base64_without_padding
  if (b64str.length() % 4 == 2) {
    b64str.append("==");
  } else if (b64str.length() % 4 == 3) {
    b64str.append("=");
  }
  return Base64Decode(b64str);
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
