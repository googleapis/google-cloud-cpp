// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PAGINATION_RANGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PAGINATION_RANGE_H

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/stream_range.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Adapt pagination APIs to look like input ranges.
 *
 * A number of gRPC APIs iterate over the elements in a "collection" using
 * pagination APIs. The application calls a `List*()` RPC which returns
 * a "page" of elements and a token, calling the same `List*()` RPC with the
 * token returns the next "page". We want to expose these APIs as input ranges
 * in the C++ client libraries.
 *
 * To construct instances of `PaginationRange<T>`, use the
 * `MakePaginationRange()` function defined below.
 *
 * @tparam T the type of the items, typically a proto describing the resources
 */
template <typename T>
using PaginationRange = StreamRange<T>;

/**
 * Returns `T`s one at a time from pages of responses.
 *
 * This class is an implementation detail. An instance of this class is wrapped
 * in a lambda and passed as the `StreamReader<T>` to the `PaginationRange<T>`
 * constructor. This class is responsible for loading pages and returning the
 * next `T`.
 *
 * Users should not use this class directly. Use the `MakePaginationRange()`
 * function (defined below) instead.
 *
 * @tparam T the type of the items, typically a proto describing the resources
 * @tparam Request the type of the request object for the `List` RPC.
 * @tparam Response the type of the response object for the `List` RPC.
 */
template <typename T, typename Request, typename Response>
class PagedStreamReader {
 public:
  /**
   * Create a new object.
   *
   * @param request the first request to start the iteration, the library may
   *    initialize this request with any filtering constraints.
   * @param loader makes the RPC request to fetch a new page of items.
   * @param extractor extracts the items from the response using native C++
   *     types (as opposed to the proto types used in `Response`).
   */
  PagedStreamReader(Request request,
                    std::function<StatusOr<Response>(Request const&)> loader,
                    std::function<std::vector<T>(Response)> extractor)
      : request_(std::move(request)),
        loader_(std::move(loader)),
        extractor_(std::move(extractor)),
        last_page_(false) {
    current_ = page_.begin();
  }

  /**
   * Fetches (or returns if already fetched) the next object from the stream.
   *
   * @return the next available `T`, if one exists (or can be loaded). Returns
   *   a non-OK `Status` to indicate an error, and an OK `Status` to indicate a
   *   successful end of stream.
   */
  typename StreamReader<T>::result_type GetNext() {
    if (current_ == page_.end()) {
      if (last_page_) return Status{};
      request_.set_page_token(std::move(token_));
      auto response = loader_(request_);
      if (!response.ok()) return std::move(response).status();
      token_ = ExtractPageToken(*response);
      if (token_.empty()) last_page_ = true;
      page_ = extractor_(*std::move(response));
      current_ = page_.begin();
      if (current_ == page_.end()) return Status{};
    }
    return std::move(*current_++);
  }

 private:
  /**
   * ExtractPageToken() extracts (i.e., "moves") the page token out of the
   * given response object. This function is overloaded based on whether the
   * response object has a `.mutable_next_page_token()` member function (e.g.,
   * a protobuf), or a `.next_page_token` field (e.g., a regular struct such as
   * is used in GCS).
   */
  template <typename U>
  static constexpr auto ExtractPageToken(U& u)
      -> decltype(std::move(*u.mutable_next_page_token())) {
    return std::move(*u.mutable_next_page_token());
  }
  template <typename U>
  static constexpr auto ExtractPageToken(U& u)
      -> decltype(std::move(u.next_page_token)) {
    return std::move(u.next_page_token);
  }

  Request request_;
  std::function<StatusOr<Response>(Request const&)> loader_;
  std::function<std::vector<T>(Response)> extractor_;
  std::vector<T> page_;
  typename std::vector<T>::iterator current_;
  std::string token_;
  bool last_page_;
};

/**
 * A factory function for creating `PaginationRange<T>` instances.
 *
 * This function creates a `PaginationRange<T>` instance that will be fed from
 * a `PagedStreamReader` (defined above).
 *
 * @par Example
 *
 * @code
 *  auto loader = [](MyRequestProto const& r) -> StatusOr<MyResponseProto> {
 *    ...
 *  };
 *  auto extractor = [](MyResponseProto r) -> std::vector<Foo> {
 *    ...
 *  };
 *  using MyFooRange = PaginationRange<Foo>;
 *  auto range = MakePaginationRange<MyFooRange>(
 *      MyRequestProto{}, std::move(loader), std::move(extractor));
 * @endcode
 */
template <typename Range, typename Request, typename Loader, typename Extractor>
Range MakePaginationRange(Request request, Loader loader, Extractor extractor) {
  using ValueType = typename Range::value_type::value_type;
  using LoaderResult = invoke_result_t<Loader, Request>;
  using Response = typename LoaderResult::value_type;
  using ExtractorResult = invoke_result_t<Extractor, Response>;
  // Some static asserts to make compiler errors easier to diagnose.
  static_assert(std::is_same<Range, PaginationRange<ValueType>>::value,
                "Expected Range is of type PaginationRange<ValueType>");
  static_assert(std::is_same<LoaderResult, StatusOr<Response>>::value,
                "Expected loader functor like StatusOr<Response>(Request)");
  static_assert(std::is_same<ExtractorResult, std::vector<ValueType>>::value,
                "Expected extractor functor like vector<ValueType>(Response)");
  using ReaderType = PagedStreamReader<ValueType, Request, Response>;
  auto reader = std::make_shared<ReaderType>(
      std::move(request), std::move(loader), std::move(extractor));
  return MakeStreamRange<ValueType>(
      {[reader]() mutable { return reader->GetNext(); }});
}

/**
 * A convenient function to make a `PaginationRange<T>` that contains a single
 * error indicating "unimplemented".
 */
template <typename Range>
Range MakeUnimplementedPaginationRange() {
  using ValueType = typename Range::value_type::value_type;
  return MakeStreamRange<ValueType>(
      []() -> typename StreamReader<ValueType>::result_type {
        return Status{StatusCode::kUnimplemented, "needs-override"};
      });
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PAGINATION_RANGE_H
