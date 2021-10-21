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

#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/internal/throw_delegate.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
// ListObjectsReader::iterator must satisfy the requirements of an
// InputIterator.
static_assert(
    std::is_same<
        std::iterator_traits<ListObjectsReader::iterator>::iterator_category,
        std::input_iterator_tag>::value,
    "ListObjectsReader::iterator should be an InputIterator");
static_assert(
    std::is_same<std::iterator_traits<ListObjectsReader::iterator>::value_type,
                 StatusOr<ObjectMetadata>>::value,
    "ListObjectsReader::iterator should be an InputIterator of ObjectMetadata");
static_assert(
    std::is_same<std::iterator_traits<ListObjectsReader::iterator>::pointer,
                 StatusOr<ObjectMetadata>*>::value,
    "ListObjectsReader::iterator should be an InputIterator of ObjectMetadata");
static_assert(
    std::is_same<std::iterator_traits<ListObjectsReader::iterator>::reference,
                 StatusOr<ObjectMetadata>&>::value,
    "ListObjectsReader::iterator should be an InputIterator of ObjectMetadata");
static_assert(std::is_copy_constructible<ListObjectsReader::iterator>::value,
              "ListObjectsReader::iterator must be CopyConstructible");
static_assert(std::is_move_constructible<ListObjectsReader::iterator>::value,
              "ListObjectsReader::iterator must be MoveConstructible");
static_assert(std::is_copy_assignable<ListObjectsReader::iterator>::value,
              "ListObjectsReader::iterator must be CopyAssignable");
static_assert(std::is_move_assignable<ListObjectsReader::iterator>::value,
              "ListObjectsReader::iterator must be MoveAssignable");
static_assert(std::is_destructible<ListObjectsReader::iterator>::value,
              "ListObjectsReader::iterator must be Destructible");
static_assert(
    std::is_convertible<decltype(*std::declval<ListObjectsReader::iterator>()),
                        ListObjectsReader::iterator::value_type>::value,
    "*it when it is of ListObjectsReader::iterator type must be convertible to "
    "ListObjectsReader::iterator::value_type>");
static_assert(
    std::is_same<decltype(++std::declval<ListObjectsReader::iterator>()),
                 ListObjectsReader::iterator&>::value,
    "++it when it is of ListObjectsReader::iterator type must be a "
    "ListObjectsReader::iterator &>");

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
