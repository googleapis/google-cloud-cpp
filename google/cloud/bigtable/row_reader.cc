// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/internal/legacy_row_reader_impl.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"
#include <iterator>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// RowReader::iterator must satisfy the requirements of an InputIterator.
static_assert(
    std::is_same<std::iterator_traits<RowReader::iterator>::iterator_category,
                 std::input_iterator_tag>::value,
    "RowReader::iterator should be an InputIterator");
static_assert(
    std::is_same<std::iterator_traits<RowReader::iterator>::value_type,
                 StatusOr<Row>>::value,
    "RowReader::iterator should be an InputIterator of Row");
static_assert(std::is_same<std::iterator_traits<RowReader::iterator>::pointer,
                           StatusOr<Row>*>::value,
              "RowReader::iterator should be an InputIterator of Row");
static_assert(std::is_same<std::iterator_traits<RowReader::iterator>::reference,
                           StatusOr<Row>&>::value,
              "RowReader::iterator should be an InputIterator of Row");
static_assert(std::is_copy_constructible<RowReader::iterator>::value,
              "RowReader::iterator must be CopyConstructible");
static_assert(std::is_move_constructible<RowReader::iterator>::value,
              "RowReader::iterator must be MoveConstructible");
static_assert(std::is_copy_assignable<RowReader::iterator>::value,
              "RowReader::iterator must be CopyAssignable");
static_assert(std::is_move_assignable<RowReader::iterator>::value,
              "RowReader::iterator must be MoveAssignable");
static_assert(std::is_destructible<RowReader::iterator>::value,
              "RowReader::iterator must be Destructible");
static_assert(
    std::is_convertible<decltype(*std::declval<RowReader::iterator>()),
                        RowReader::iterator::value_type>::value,
    "*it when it is of RowReader::iterator type must be convertible to "
    "RowReader::iterator::value_type>");
static_assert(std::is_same<decltype(++std::declval<RowReader::iterator>()),
                           RowReader::iterator&>::value,
              "++it when it is of RowReader::iterator type must be a "
              "RowReader::iterator &>");

RowReader::RowReader()
    : RowReader(
          std::make_shared<bigtable_internal::StatusOnlyRowReader>(Status{})) {}

RowReader::RowReader(
    std::shared_ptr<DataClient> client, std::string table_name, RowSet row_set,
    std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
    : RowReader(std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
          std::move(client), std::move(table_name), std::move(row_set),
          rows_limit, std::move(filter), std::move(retry_policy),
          std::move(backoff_policy), std::move(metadata_update_policy),
          std::move(parser_factory))) {}

RowReader::RowReader(
    std::shared_ptr<DataClient> client, std::string app_profile_id,
    std::string table_name, RowSet row_set, std::int64_t rows_limit,
    Filter filter, std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
    : RowReader(std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
          std::move(client), std::move(app_profile_id), std::move(table_name),
          std::move(row_set), rows_limit, std::move(filter),
          std::move(retry_policy), std::move(backoff_policy),
          std::move(metadata_update_policy), std::move(parser_factory))) {}

std::int64_t constexpr RowReader::NO_ROWS_LIMIT;

// The name must be all lowercase to work with range-for loops.
RowReader::iterator RowReader::begin() {
  auto impl = impl_;
  stream_ = google::cloud::internal::MakeStreamRange<Row>(
      [impl] { return impl->Advance(); });
  return stream_.begin();
}

// The name must be all lowercase to work with range-for loops.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
RowReader::iterator RowReader::end() { return stream_.end(); }

void RowReader::Cancel() { impl_->Cancel(); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
