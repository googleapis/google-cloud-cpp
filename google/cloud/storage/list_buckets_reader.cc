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

#include "google/cloud/storage/list_buckets_reader.h"
#include "google/cloud/internal/throw_delegate.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
// ListBucketsReader::iterator must satisfy the requirements of an
// InputIterator.
static_assert(
    std::is_same<
        std::iterator_traits<ListBucketsReader::iterator>::iterator_category,
        std::input_iterator_tag>::value,
    "ListBucketsReader::iterator should be an InputIterator");
static_assert(
    std::is_same<std::iterator_traits<ListBucketsReader::iterator>::value_type,
                 StatusOr<BucketMetadata>>::value,
    "ListBucketsReader::iterator should be an InputIterator of BucketMetadata");
static_assert(
    std::is_same<std::iterator_traits<ListBucketsReader::iterator>::pointer,
                 StatusOr<BucketMetadata>*>::value,
    "ListBucketsReader::iterator should be an InputIterator of BucketMetadata");
static_assert(
    std::is_same<std::iterator_traits<ListBucketsReader::iterator>::reference,
                 StatusOr<BucketMetadata>&>::value,
    "ListBucketsReader::iterator should be an InputIterator of BucketMetadata");
static_assert(std::is_copy_constructible<ListBucketsReader::iterator>::value,
              "ListBucketsReader::iterator must be CopyConstructible");
static_assert(std::is_move_constructible<ListBucketsReader::iterator>::value,
              "ListBucketsReader::iterator must be MoveConstructible");
static_assert(std::is_copy_assignable<ListBucketsReader::iterator>::value,
              "ListBucketsReader::iterator must be CopyAssignable");
static_assert(std::is_move_assignable<ListBucketsReader::iterator>::value,
              "ListBucketsReader::iterator must be MoveAssignable");
static_assert(std::is_destructible<ListBucketsReader::iterator>::value,
              "ListBucketsReader::iterator must be Destructible");
static_assert(
    std::is_convertible<decltype(*std::declval<ListBucketsReader::iterator>()),
                        ListBucketsReader::iterator::value_type>::value,
    "*it when it is of ListBucketsReader::iterator type must be convertible to "
    "ListBucketsReader::iterator::value_type>");
static_assert(
    std::is_same<decltype(++std::declval<ListBucketsReader::iterator>()),
                 ListBucketsReader::iterator&>::value,
    "++it when it is of ListBucketsReader::iterator type must be a "
    "ListBucketsReader::iterator &>");

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
