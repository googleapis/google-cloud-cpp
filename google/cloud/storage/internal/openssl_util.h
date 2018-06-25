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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OPENSSL_UTIL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OPENSSL_UTIL_H_

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/internal/credential_constants.h"
#include "google/cloud/storage/version.h"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
#if OPENSSL_VERSION_NUMBER < 0x10100000L  // Older than version 1.1.0
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

/**
 * Helper functions for Base64 and related transcoding.
 */
struct OpenSslUtils {
 public:
  /**
   * Encode a string using Base64.
   */
  static std::string Base64Encode(const std::string& str) {
    auto bio_chain = MakeBioChainForBase64Transcoding();
    int retval = 0;

    while (true) {
      retval = BIO_write(static_cast<BIO*>(bio_chain.get()), str.c_str(),
                         static_cast<int>(str.length()));
      if (retval > 0) break;  // Positive value == successful write.
      if (!BIO_should_retry(static_cast<BIO*>(bio_chain.get()))) {
        std::ostringstream err_builder;
        err_builder << "Permanent error in " << __func__ << ": "
                    << "BIO_write returned non-retryable value of " << retval;
        google::cloud::internal::RaiseRuntimeError(err_builder.str());
      }
    }
    // Tell the b64 encoder that we're done writing data, thus prompting it to
    // add trailing '=' characters for padding if needed.
    while (true) {
      retval = BIO_flush(static_cast<BIO*>(bio_chain.get()));
      if (retval > 0) break;  // Positive value == successful flush.
      if (!BIO_should_retry(static_cast<BIO*>(bio_chain.get()))) {
        std::ostringstream err_builder;
        err_builder << "Permanent error in " << __func__ << ": "
                    << "BIO_flush returned non-retryable value of " << retval;
        google::cloud::internal::RaiseRuntimeError(err_builder.str());
      }
    }

    // This buffer belongs to the BIO chain and is freed upon its destruction.
    BUF_MEM* buf_mem;
    BIO_get_mem_ptr(static_cast<BIO*>(bio_chain.get()), &buf_mem);
    // Return a string copy of the buffer's bytes, as the buffer will be freed
    // upon this method's exit.
    return std::string(buf_mem->data, buf_mem->length);
  }

  /**
   * Transform a string in-place, removing trailing occurrences of a character.
   *
   * Warning: this was written with the intent of operating on a string
   * containing ASCII-encoded (8-bit) characters (e.g. removing trailing '='
   * characters from a Base64-encoded string) and may not function correctly
   * for strings containing Unicode characters.
   */
  static std::string& RightTrim(std::string& str, const char& trim_ch) {
    str.erase(
        std::find_if(str.rbegin(), str.rend(),
                     [&trim_ch](char cur_ch) { return trim_ch != cur_ch; })
            .base(),
        str.end());
    return str;
  }

  /**
   * Sign a string with the private key from a PEM container.
   */
  static std::string SignStringWithPem(
      const std::string& str, const std::string& pem_contents,
      google::cloud::storage::internal::JWTSigningAlgorithms alg) {
    // We check for failures several times, so we shorten this into a lambda
    // to avoid bloating the code with alloc/init checks.
    auto func_name = __func__;  // Avoid using the lambda name instead.
    auto handle_openssl_failure = [&func_name](std::string error_msg) -> void {
      std::ostringstream err_builder;
      err_builder << "Permanent error in " << func_name
                  << " (failed to sign string with PEM key): " << std::endl
                  << error_msg;
      google::cloud::internal::RaiseRuntimeError(err_builder.str());
    };

    auto digest_ctx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
        EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (not(digest_ctx)) {
      handle_openssl_failure("Could not create context for OpenSSL digest.");
    }

    const EVP_MD* digest_type = nullptr;
    switch (alg) {
      case RS256:
        digest_type = EVP_sha256();
        break;
    }
    if (digest_type == nullptr) {
      handle_openssl_failure("Could not find specified digest in OpenSSL.");
    }

    auto pem_buffer = std::unique_ptr<BIO, decltype(&BIO_free)>(
        BIO_new_mem_buf(const_cast<char*>(pem_contents.c_str()),
                        pem_contents.length()),
        &BIO_free);
    if (not(pem_buffer)) handle_openssl_failure("Could not create PEM buffer.");

    auto private_key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(
        PEM_read_bio_PrivateKey(
            static_cast<BIO*>(pem_buffer.get()),
            nullptr,  // EVP_PKEY **x
            nullptr,  // pem_password_cb *cb -- a custom callback.
            // void *u -- this represents the password for the PEM (only
            // applicable for formats such as PKCS12 (.p12 files) that use
            // a password, which we don't currently support.
            nullptr),
        &EVP_PKEY_free);
    if (not(private_key)) {
      handle_openssl_failure("Could not parse PEM to get private key.");
    }

    const int DIGEST_SIGN_SUCCESS_CODE = 1;
    if (DIGEST_SIGN_SUCCESS_CODE !=
        EVP_DigestSignInit(static_cast<EVP_MD_CTX*>(digest_ctx.get()),
                           nullptr,  // EVP_PKEY_CTX **pctx
                           digest_type,
                           nullptr,  // ENGINE *e
                           static_cast<EVP_PKEY*>(private_key.get()))) {
      handle_openssl_failure("Could not initialize PEM digest.");
    }

    if (DIGEST_SIGN_SUCCESS_CODE !=
        EVP_DigestSignUpdate(static_cast<EVP_MD_CTX*>(digest_ctx.get()),
                             str.c_str(), str.length())) {
      handle_openssl_failure("Could not update PEM digest.");
    }

    std::size_t signed_str_size = 0;
    // Calling this method with a nullptr buffer will populate our size var
    // with, the resulting buffer's size. This allows us to then call it again,
    // with the correct buffer and size, which actually populates the buffer.
    if (DIGEST_SIGN_SUCCESS_CODE !=
        EVP_DigestSignFinal(static_cast<EVP_MD_CTX*>(digest_ctx.get()),
                            nullptr,  // unsigned char *sig
                            &signed_str_size)) {
      handle_openssl_failure("Could not finalize PEM digest (1/2).");
    }

    std::vector<unsigned char> signed_str(signed_str_size);
    if (DIGEST_SIGN_SUCCESS_CODE !=
        EVP_DigestSignFinal(static_cast<EVP_MD_CTX*>(digest_ctx.get()),
                            signed_str.data(), &signed_str_size)) {
      handle_openssl_failure("Could not finalize PEM digest (2/2).");
    }

    return std::string(signed_str.begin(),
                       signed_str.begin() + signed_str_size);
  }

  /**
   * Return a Base64-encoded version of the given a string, using the URL- and
   * filesystem-safe alphabet, making these adjustments:
   * -  Replace '+' with '-'
   * -  Replace '/' with '_'
   * -  Right-trim '=' characters
   */
  static std::string UrlsafeBase64Encode(const std::string& str) {
    std::string b64str = Base64Encode(str);
    std::replace(b64str.begin(), b64str.end(), '+', '-');
    std::replace(b64str.begin(), b64str.end(), '/', '_');
    RightTrim(b64str, '=');
    return b64str;
  }

 private:
  static std::unique_ptr<BIO, decltype(&BIO_free_all)>
  MakeBioChainForBase64Transcoding() {
    std::ostringstream err_builder;
    auto base64_io = std::unique_ptr<BIO, decltype(&BIO_free)>(
        BIO_new(BIO_f_base64()), &BIO_free);
    auto mem_io = std::unique_ptr<BIO, decltype(&BIO_free)>(
        BIO_new(BIO_s_mem()), &BIO_free);
    if (not(base64_io and mem_io)) {
      err_builder << "Permanent error in " << __func__ << ": "
                  << "Could not allocate BIO* for Base64 encoding.";
      google::cloud::internal::RaiseRuntimeError(err_builder.str());
    }
    auto bio_chain = std::unique_ptr<BIO, decltype(&BIO_free_all)>(
        // Output from a b64 encoder should go to an in-memory sink.
        BIO_push(static_cast<BIO*>(base64_io.release()),
                 static_cast<BIO*>(mem_io.release())),
        // Make sure we free all resources in this chain upon destruction.
        &BIO_free_all);
    // Don't use newlines as a signal for when to flush buffers.
    BIO_set_flags(static_cast<BIO*>(bio_chain.get()), BIO_FLAGS_BASE64_NO_NL);
    return bio_chain;
  }
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OPENSSL_UTIL_H_
