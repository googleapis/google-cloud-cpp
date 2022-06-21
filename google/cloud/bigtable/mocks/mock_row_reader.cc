// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigtable_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigtable::RowReader MakeRowReader(std::vector<bigtable::Row> rows,
                                  Status final_status) {
  class ConvenientRowReader : public bigtable_internal::RowReaderImpl {
   public:
    explicit ConvenientRowReader(std::vector<bigtable::Row> rows,
                                 Status final_status)
        : final_status_(std::move(final_status)),
          rows_(std::move(rows)),
          iter_(rows_.cbegin()) {}

    ~ConvenientRowReader() override = default;

    /// Skips remaining rows and invalidates current iterator.
    void Cancel() override { iter_ = rows_.cend(); };

    absl::variant<Status, bigtable::Row> Advance() override {
      if (iter_ == rows_.cend()) return final_status_;
      return *iter_++;
    }

   private:
    Status final_status_;
    std::vector<bigtable::Row> const rows_;
    std::vector<bigtable::Row>::const_iterator iter_;
  };

  auto impl = std::make_shared<ConvenientRowReader>(std::move(rows),
                                                    std::move(final_status));
  return bigtable_internal::MakeRowReader(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_mocks
}  // namespace cloud
}  // namespace google
