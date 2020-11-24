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

#include "google/cloud/internal/stream_range.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <functional>
#include <string>
#include <utility>
#include <memory>
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
 * in the C++ client libraries. This class performs that work.
 *
 * @tparam T the type of the items, typically a proto describing the resources
 * @tparam Request the type of the request object for the `List` RPC.
 * @tparam Response the type of the response object for the `List` RPC.
 */
template <typename T, typename Request, typename Response>
class PagedStreamReader {
 public:
  /**
   * Create a new range to paginate over some elements.
   *
   * @param request the first request to start the iteration, the library may
   *    initialize this request with any filtering constraints.
   * @param loader makes the RPC request to fetch a new page of items.
   * @param get_items extracts the items from the response using native C++
   *     types (as opposed to the proto types used in `Response`).
   */
   PagedStreamReader(Request request,
             std::function<StatusOr<Response>(Request const& r)> loader,
             std::function<std::vector<T>(Response r)> get_items)
       : request_(std::move(request)),
         next_page_loader_(std::move(loader)),
         get_items_(std::move(get_items)),
         on_last_page_(false) {
     current_ = current_page_.begin();
   }

  /**
   * Fetches (or returns if already fetched) the next object from the stream.
   *
   * @return An iterator pointing to the next element in the stream. On error,
   *   it returns an iterator that is different from `.end()`, but has an error
   *   status. If the stream is exhausted, it returns the `.end()` iterator.
   */
  typename StreamReader<T>::result_type GetNext() {
    if (current_page_.end() == current_) {
      if (on_last_page_) return Status{};  // End of stream
      request_.set_page_token(std::move(next_page_token_));
      auto response = next_page_loader_(request_);
      if (!response.ok()) {
        next_page_token_.clear();
        current_page_.clear();
        on_last_page_ = true;
        current_ = current_page_.begin();
        return std::move(response).status();
      }
      next_page_token_ = std::move(*response->mutable_next_page_token());
      current_page_ = get_items_(*std::move(response));
      current_ = current_page_.begin();
      if (next_page_token_.empty()) on_last_page_ = true;
      if (current_page_.end() == current_) return Status{};  // End of stream
    }
    return std::move(*current_++);
  }

 private:
  Request request_;
  std::function<StatusOr<Response>(Request const& r)> next_page_loader_;
  std::function<std::vector<T>(Response r)> get_items_;
  std::vector<T> current_page_;
  typename std::vector<T>::iterator current_;
  std::string next_page_token_;
  bool on_last_page_;
};

template <typename T>
using PaginationRange = StreamRange<T>;

template <typename Range, typename Request, typename Loader, typename Extractor,
          typename ValueType = typename Range::value_type::value_type,
          typename Response =
              typename std::result_of<Loader(Request)>::type::value_type>
Range MakePaginationRange(Request request, Loader loader, Extractor extractor) {
  auto reader =
      std::make_shared<PagedStreamReader<ValueType, Request, Response>>(
          std::move(request), std::move(loader), std::move(extractor));
  return Range([reader]() mutable ->
               typename StreamReader<ValueType>::result_type {
                 return reader->GetNext();
               });
}

template <typename Range>
Range MakeUnimplementedPaginationRange() {
  return Range{[]() -> typename StreamReader<
                        typename Range::value_type::value_type>::result_type {
    return Status{StatusCode::kUnimplemented, "needs-override"};
  }};
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PAGINATION_RANGE_H
