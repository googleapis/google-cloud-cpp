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

#include "google/cloud/storage/list_hmac_keys_reader.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
// ListHmacKeysReader::iterator must satisfy the requirements of an
// InputIterator.
static_assert(
    std::is_same<
        std::iterator_traits<ListHmacKeysReader::iterator>::iterator_category,
        std::input_iterator_tag>::value,
    "ListHmacKeysReader::iterator should be an InputIterator");
static_assert(
    std::is_same<std::iterator_traits<ListHmacKeysReader::iterator>::value_type,
                 StatusOr<HmacKeyMetadata>>::value,
    "ListHmacKeysReader::iterator should be an InputIterator of "
    "HmacKeyMetadata");
static_assert(
    std::is_same<std::iterator_traits<ListHmacKeysReader::iterator>::pointer,
                 StatusOr<HmacKeyMetadata>*>::value,
    "ListHmacKeysReader::iterator should be an InputIterator of "
    "HmacKeyMetadata");
static_assert(
    std::is_same<std::iterator_traits<ListHmacKeysReader::iterator>::reference,
                 StatusOr<HmacKeyMetadata>&>::value,
    "ListHmacKeysReader::iterator should be an InputIterator of "
    "HmacKeyMetadata");
static_assert(std::is_copy_constructible<ListHmacKeysReader::iterator>::value,
              "ListHmacKeysReader::iterator must be CopyConstructible");
static_assert(std::is_move_constructible<ListHmacKeysReader::iterator>::value,
              "ListHmacKeysReader::iterator must be MoveConstructible");
static_assert(std::is_copy_assignable<ListHmacKeysReader::iterator>::value,
              "ListHmacKeysReader::iterator must be CopyAssignable");
static_assert(std::is_move_assignable<ListHmacKeysReader::iterator>::value,
              "ListHmacKeysReader::iterator must be MoveAssignable");
static_assert(std::is_destructible<ListHmacKeysReader::iterator>::value,
              "ListHmacKeysReader::iterator must be Destructible");
static_assert(
    std::is_convertible<decltype(*std::declval<ListHmacKeysReader::iterator>()),
                        ListHmacKeysReader::iterator::value_type>::value,
    "*it when it is of ListHmacKeysReader::iterator type must be convertible "
    "to ListHmacKeysReader::iterator::value_type>");
static_assert(
    std::is_same<decltype(++std::declval<ListHmacKeysReader::iterator>()),
                 ListHmacKeysReader::iterator&>::value,
    "++it when it is of ListHmacKeysReader::iterator type must be a "
    "ListHmacKeysReader::iterator &>");

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
