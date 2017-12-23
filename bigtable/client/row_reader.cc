// Copyright 2017 Google Inc.
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

#include "bigtable/client/row_reader.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {}  // namespace BIGTABLE_CLIENT_NS

RowReader::iterator RowReader::begin() {
  Advance();
  if (rows_.empty()) {
    return end();
  }
  return RowReader::RowReaderIterator(this, false);
}

RowReader::iterator RowReader::end() {
  return RowReader::RowReaderIterator(this, true);
}

RowReader::RowReaderIterator& RowReader::RowReaderIterator::operator++() {
  owner_->Advance();
  return *this;
}

void RowReader::Advance() {}

}  // namespace bigtable
