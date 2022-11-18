// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ERROR_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ERROR_METADATA_H

#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A (relatively) lightweight data structure to pass error metadata across
 * implementation functions.
 *
 * Sometimes we want to provide additional context about errors. The original
 * motivation is credential file parsing. These files can be fairly complex,
 * and parsing requires many functions that only need the *contents* of the
 * file to parse it, but may want to show the filename, the start of the
 * parsing call tree, and maybe some key intermediate callers.
 *
 * This class can be used to pass these additional parameters as needed, maybe
 * growing as parsing partially succeeds, and if there is an error the data
 * can be included as part of the message (or `google::cloud::ErrorInfo`).
 *
 * @par Example
 * @code
 * namespace google::cloud::internal {
 * StatusOr<Foo> ParseFooFile(std::string filename) {
 *   ErrorContext ec{
 *     {"filename", filename},
 *     {"origin", __func__},
 *   };
 *   std::ifstream is(filename);
 *   auto contents = std::string{std::istreambuf_iterator<char>{is.buf()},{ }};
 *   if (is.bad()) {
 *     return Status(
 *        StatusCode::kInvalidArgument,
 *        Format("cannot read file", error_context));
 *   }
 *   return ParseFooFileContents(std::move(contents), std::move(ec));
 * }
 *
 * StatusOr<Foo> ParseFooFileContents(std::string contents, ErrorContext ec) {
 *   // Do some stuff
 *   if (has_bar()) return ParseFooFileContentsWithBar(
 *       .., std::move(error_context));
 *   // do more stuff
 *   if (bad()) return Status(
 *       StatusCode::kInvalidArgument,
 *       Format("badness parsing thingamajig", ec));
 *   // all good
 *   return Foo{...};
 * }
 * @endcode
 */
class ErrorContext {
 public:
  using Container = std::vector<std::pair<std::string, std::string>>;

  ErrorContext() = default;
  explicit ErrorContext(Container m) : metadata_(std::move(m)) {}

  ErrorContext(ErrorContext const&) = default;
  ErrorContext& operator=(ErrorContext const&) = default;
  ErrorContext(ErrorContext&&) = default;
  ErrorContext& operator=(ErrorContext&&) = default;

  template <typename... A>
  Container::reference emplace_back(A&&... a) {
    return metadata_.emplace_back(std::forward<A>(a)...);
  }

  void push_back(Container::value_type p) {
    return metadata_.push_back(std::move(p));
  }

  bool empty() const { return metadata_.empty(); }

  Container::const_iterator begin() const { return metadata_.begin(); }

  Container::const_iterator end() const { return metadata_.end(); }

 private:
  Container metadata_;
};

std::string Format(absl::string_view message, ErrorContext const& context);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ERROR_METADATA_H
